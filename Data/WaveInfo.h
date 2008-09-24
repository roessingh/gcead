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

#ifndef __WAVEINFO_H
#define __WAVEINFO_H

#include <QObject>
#include <QRect>
#include <QString>
#include <QVector>


class RecInfo;


enum WaveType
{
	WaveType_EAD,
	WaveType_FID,
	WaveType_Digital,
};
const int WaveTypeCount = WaveType_Digital + 1;


class WavePoint
{
public:
	int i;
	double n;

	WavePoint()
		: i(-1)
	{
	}

	WavePoint(int i, double n)
		: i(i), n(n)
	{
	}
};

class WavePeakInfo
{
public:
	bool bEnabled;
	WavePoint left;
	WavePoint middle;
	WavePoint right;
};

class WavePeakChosenInfo
{
public:
	int didxLeft;
	int didxMiddle;
	int didxRight;
	double nArea;
	double nPercent;
};


class WavePos
{
public:
	/// Is this waveform visible?
	bool bVisible;
	/// Millivolts per division
	double nVoltsPerDivision;
	/// Standard offset in divisions from top (value is generally between 0 and 10)
	double nDivisionOffset;

	WavePos()
		: bVisible(true), nVoltsPerDivision(1), nDivisionOffset(5)
	{
	}
};


/// Graph data that's independent of the view mode.
class WaveInfo : public QObject
{
public:
	/// Raw data
	QVector<short> raw;
	/// Numerator
	int nRawToVoltageFactorNum;
	/// Denominator
	int nRawToVoltageFactorDen;
	/// Factor to multiply raw data by to get volts
	/// This value needs to be explicitly set as:
	/// nRawToVoltageFactor = double(nRawToVoltageFactorNum) / nRawToVoltageFactorDen;
	double nRawToVoltageFactor;
	/// Display data
	QVector<double> display;
	/// List of possible peaks
	QList<WavePeakInfo> peaks0;
	/// List of chosen peaks
	QList<WavePeakChosenInfo> peaksChosen;
	WaveType type;
	QString sName;
	QString sComment;
	/// Offset and sensitivity
	WavePos pos;

public:
	WaveInfo(RecInfo* rec);

	RecInfo* rec() { return m_rec; }
	const RecInfo* rec() const { return m_rec; }
	
	int recId() const;
	/// Number of samples to shift the display this wave to the right.
	/// Negative values shift the dataset to the left.
	int shift() const;
	void setShift(int nShift);

	/// Convert the raw data to display data
	void calcDisplayData();

	void findFidPeaks();

	void choosePeakAtDidx(int didx);
	void unchoosePeakAtDidx(int didx);

	/// Call this if you've changed the indexes of a peak in order to recalculate the area under the curve.
	void calcPeakArea(int iPeak);
	/// This calculates the areas of all peaks and implicitly calls calcAreaPercents().
	/// This method should be called when a WaveInfo object is loaded from storage.
	void calcPeakAreas();
	/// Calculate the area percents
	void calcAreaPercents();

private:
	RecInfo* m_rec;
	int m_nShift;
};

#endif
