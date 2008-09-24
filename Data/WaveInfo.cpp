/**
 * Copyright (C) 2008  Ellis Whitehead
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "WaveInfo.h"

#include <QtDebug>

#include <Check.h>
#include <EadEnums.h>

#include "RecInfo.h"


#define RADIUS (EAD_SAMPLES_PER_SECOND / 2)


WaveInfo::WaveInfo(RecInfo* rec)
{
	Q_ASSERT(rec != NULL);
	m_rec = rec;

	nRawToVoltageFactorNum = 1;
	nRawToVoltageFactorDen = 1;
	nRawToVoltageFactor = 1;
	m_nShift = 0;
}

int WaveInfo::recId() const
{
	return m_rec->id();
}

int WaveInfo::shift() const
{
	//return m_rec->shift();
	return m_nShift;
}

void WaveInfo::setShift(int nShift)
{
	m_nShift = nShift;
}

void WaveInfo::calcDisplayData()
{
	const short* orig = raw.constData();
	
	display.resize(raw.size());
	double* changed = display.data();

	for (int i = 0; i < raw.size(); i++)
	{
		double n = *orig++;
		n *= nRawToVoltageFactor;
		*changed++ = n;
	}
}

void WaveInfo::findFidPeaks()
{
	// First find all maximums
	QList<WavePoint> peaksAll;
	const double* data = display.constData();
	for (int i = RADIUS; i < display.size() - RADIUS; i++)
	{
		double n = data[i];
		double nComp = n + 1e-10;

		// Find whether there is a peak at 'i' with RADIUS samples
		bool bMax = true;
		for (int j = i - RADIUS; j < i; j++)
		{
			if (data[j] >= nComp)
			{
				bMax = false;
				break;
			}
		}
		for (int j = i + 1; j <= i + RADIUS; j++)
		{
			if (data[j] >= nComp)
			{
				bMax = false;
				break;
			}
		}

		if (bMax)
		{
			//qDebug() << "Peak:" << i << n;
			peaksAll << WavePoint(i, n);
			i += RADIUS / 2;
		}
	}

	WavePeakInfo info;
	peaks0.clear();
	foreach (WavePoint pt, peaksAll)
	{
		{
			// Distance to the left from pt.i
			int nIncreasing = 0;
			double nMin = pt.n;
			double nPrev = pt.n;
			int iMin = pt.i;
			int i = pt.i;
			int nSamples = 1;
			while(true)
			{
				i--;
				nSamples++;

				double n = display[i];
				double nComp = n - 1e-10;
				if (n < nMin)
				{
					nMin = n;
					iMin = i;
				}
				if (nComp <= nPrev)
					nIncreasing++;

				nPrev = n;

				double nFraction = double(nIncreasing) / nSamples;
				if (i <= 0)
					break;
				if (nSamples > RADIUS / 2 && nFraction < 0.8)
					break;
			}

			double nMinThreshold = nMin + (pt.n - nMin) * 0.05;
			for (i = pt.i - 1; i > iMin; i--)
			{
				if (display[i] <= nMinThreshold)
					break;
			}
			i++;

			info.left = WavePoint(i, display[i]);
		}

		{
			// Distance to the right from pt.i
			int nDecreasing = 0;
			double nMin = pt.n;
			double nPrev = pt.n;
			int iMin = pt.i;
			int i = pt.i;
			int nSamples = 1;
			while (true)
			{
				i++;
				nSamples++;
				double n = display[i];
				double nComp = n - 1e-10;
				if (n < nMin)
				{
					nMin = n;
					iMin = i;
				}
				if (nComp <= nPrev)
					nDecreasing++;

				nPrev = n;
				
				double nFraction = double(nDecreasing) / nSamples;
				if (i == display.size() - 1)
					break;
				if (nSamples > RADIUS / 2 && nFraction < 0.8)
					break;
			}
			
			double nMinThreshold = nMin + (pt.n - nMin) * 0.05;
			for (i = pt.i + 1; i < iMin; i++)
			{
				if (display[i] <= nMinThreshold)
					break;
			}
			i--;

			info.right = WavePoint(i, display[i]);
		}

		//qDebug() << "Width:" << info.left.i << pt.i << info.right.i;

		if (info.left.i + RADIUS < pt.i && info.right.i - RADIUS > pt.i)
		{
			// Check whether this peak has already been chosen by the user:
			bool bChosen = false;
			for (int iChosen = 0; iChosen < peaksChosen.size(); iChosen++)
			{
				if (peaksChosen[iChosen].didxMiddle == pt.i)
				{
					bChosen = true;
					break;
				}
			}

			info.bEnabled = !bChosen;
			info.middle = pt;
			//qDebug() << "\tOK";
			peaks0 << info;
		}
	}
}

/*
THIS IS FOR findFidPeaks() WITH RAW SHORT DATA
void WaveInfo::findFidPeaks()
{
	// First find all maximums
	QList<WavePoint> peaksAll;
	for (int i = RADIUS; i < raw.size() - RADIUS; i++)
	{
		short n = raw[i];
		
		// Find whether there is a peak at 'i' with RADIUS samples
		bool bMax = true;
		for (int j = i - RADIUS; j < i; j++)
		{
			if (raw[j] >= n)
			{
				bMax = false;
				break;
			}
		}
		for (int j = i + 1; j <= i + RADIUS; j++)
		{
			if (raw[j] >= n)
			{
				bMax = false;
				break;
			}
		}

		if (bMax)
		{
			peaksAll << WavePoint(i, n);
		}
	}

	WavePeakInfo info;
	peaks0.clear();
	foreach (WavePoint pt, peaksAll)
	{
		{
			// Distance to the left from pt.i
			int nIncreasing = 0;
			short nMin = pt.n;
			short nPrev = pt.n;
			int iMin = pt.i;
			int i = pt.i;
			do
			{
				i--;
				short n = raw[i];
				if (n < nMin)
				{
					nMin = n;
					iMin = i;
				}
				if (n >= nPrev)
					nIncreasing++;

				nPrev = n;
			} while (i > 0 && double(nIncreasing) / (pt.i - i + 1) <= 0.2);

			short nMinThreshold = nMin + (pt.n - nMin) / 20;
			for (i = pt.i - 1; i > iMin; i--)
			{
				if (raw[i] <= nMinThreshold)
					break;
			}
			i++;

			info.left = WavePoint(i, raw[i]);
		}

		{
			// Distance to the right from pt.i
			int nDecreasing = 0;
			short nMin = pt.n;
			short nPrev = pt.n;
			int iMin = pt.i;
			int i = pt.i;
			do
			{
				i++;
				short n = raw[i];
				if (n < nMin)
				{
					nMin = n;
					iMin = i;
				}
				if (n >= nPrev)
					nDecreasing++;

				nPrev = n;
			} while (i < raw.size() - 1 && double(nDecreasing) / (i - pt.i + 1) <= 0.2);
			
			short nMinThreshold = nMin + (pt.n - nMin) / 20;
			for (i = pt.i + 1; i < iMin; i++)
			{
				if (raw[i] <= nMinThreshold)
					break;
			}
			i--;

			info.right = WavePoint(i, raw[i]);
		}

		if (info.left.i + RADIUS < pt.i && info.right.i - RADIUS > pt.i)
		{
			// Check whether this peak has already been chosen by the user:
			bool bChosen = false;
			for (int iChosen = 0; iChosen < peaksChosen.size(); iChosen++)
			{
				if (peaksChosen[iChosen].didxMiddle == pt.i)
				{
					bChosen = true;
					break;
				}
			}

			info.bEnabled = !bChosen;
			info.middle = pt;
			peaks0 << info;
		}
	}
}

*/

void WaveInfo::choosePeakAtDidx(int didx)
{
	for (int i0 = 0; i0 < peaks0.size(); i0++)
	{
		if (peaks0.at(i0).middle.i == didx)
		{
			peaks0[i0].bEnabled = false;
			
			WavePeakChosenInfo peak;
			peak.didxLeft = peaks0[i0].left.i;
			peak.didxMiddle = peaks0[i0].middle.i;
			peak.didxRight = peaks0[i0].right.i;
			peaksChosen << peak;
			
			calcPeakArea(peaksChosen.size() - 1);
			calcAreaPercents();
			return;
		}
	}

	CHECK_FAILURE_RET();
}

void WaveInfo::unchoosePeakAtDidx(int didx)
{
	for (int i = 0; i < peaksChosen.size(); i++)
	{
		if (peaksChosen.at(i).didxMiddle == didx)
		{
			peaksChosen.removeAt(i);

			// Enable the possible peak again
			for (int i0 = 0; i0 < peaks0.size(); i0++)
			{
				if (peaks0.at(i0).middle.i == didx)
				{
					peaks0[i0].bEnabled = true;
					break;
				}
			}

			return;
		}
	}
}

void WaveInfo::calcPeakArea(int iPeak)
{
	CHECK_PARAM_RET(iPeak >= 0 && iPeak < peaksChosen.size());

	WavePeakChosenInfo& peak = peaksChosen[iPeak];
	
	// Sum area under the curve
	double nArea = 0;
	for (int didx = peak.didxLeft; didx <= peak.didxRight; didx++)
	{
		nArea += display[didx];
	}

	// Subtract area beneath the area line (area_of_square / 2)
	double nHeight = qAbs(display[peak.didxLeft] - display[peak.didxRight]);
	int nWidth = peak.didxRight - peak.didxLeft + 1;
	nArea -= (nHeight * nWidth) / 2;

	peak.nArea = nArea;
}

void WaveInfo::calcPeakAreas()
{
	for (int i = 0; i < peaksChosen.size(); i++)
		calcPeakArea(i);
	calcAreaPercents();
}

void WaveInfo::calcAreaPercents()
{
	double nTotal = 0;
	for (int i = 0; i < peaksChosen.size(); i++)
	{
		nTotal += peaksChosen[i].nArea;
	}
	for (int i = 0; i < peaksChosen.size(); i++)
	{
		peaksChosen[i].nPercent = peaksChosen[i].nArea / nTotal;
	}
}
