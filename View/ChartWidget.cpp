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

#include "ChartWidget.h"

#include <math.h>

#include <QtDebug>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QStatusBar>
#include <QTimer>
#include <QToolButton>

#include <WaveInfo.h>
#include "Actions.h"
#include "ChartPixmap.h"
#include <ChartScope.h>
#include "Check.h"
#include "EadFile.h"
#include "Globals.h"
#include "MainScope.h"
#include "PublisherSettings.h"
#include "Utils.h"
#include "ViewSettings.h"
#include "WaveEditorDialog.h"


ChartWidget::ChartWidget(MainScope* mainS, QWidget* parent)
	: QWidget(parent)
{
	Q_ASSERT(mainS != NULL);
	Q_ASSERT(mainS->chart() != NULL);

	setMouseTracking(true);
	setFocusPolicy(Qt::StrongFocus);

	m_mainS = mainS;
	m_chartS = mainS->chart();
	m_pixmap = m_chartS->pixmap();
	connect(m_chartS, SIGNAL(paramsChanged()), this, SLOT(repaintChart()));
	connect(m_chartS, SIGNAL(recordingLabelVisibleChanged(bool)), m_lblRecording, SLOT(setVisible(bool)));
	// FIXME: most of these will need to be changed -- ellis, 2009-10-25
	connect(m_mainS, SIGNAL(waveListChanged()), this, SLOT(scopeChanged()));
	connect(m_mainS, SIGNAL(peakModeChanged(EadPeakMode)), this, SLOT(scopeChanged()));
	connect(m_mainS, SIGNAL(updateRecordings()), this, SLOT(updateRecordings()));
	connect(m_mainS, SIGNAL(viewSettingChanged(const QString&)), this, SLOT(scopeChanged()));

	m_statusbar = NULL;
	m_lblStatus = NULL;

	m_bDragging = false;
	m_bSelecting = false;

	m_recordingEadWave = NULL;
	m_recordingFidWave = NULL;

	setupWidgets();

	m_timerUpdate = new QTimer(this);
	connect(m_timerUpdate, SIGNAL(timeout()), this, SLOT(on_timerUpdate_timeout()));
}

ChartWidget::~ChartWidget()
{
}

void ChartWidget::setupWidgets()
{
	m_buttons = new QWidget(this);

	QHBoxLayout* tblayout = new QHBoxLayout;
	tblayout->addItem(new QSpacerItem(15, 10, QSizePolicy::Fixed, QSizePolicy::Fixed));
	tblayout->addWidget(new QLabel(tr("Timebase:")));
	tblayout->addItem(new QSpacerItem(5, 5, QSizePolicy::Fixed, QSizePolicy::Fixed));
	m_lblSecondsPerDivision = new QLabel();
	tblayout->addWidget(m_lblSecondsPerDivision);

	tblayout->addSpacing(15);

	QToolButton* tbtn;

	tbtn = new QToolButton;
	tbtn->setDefaultAction(m_mainS->actions()->viewZoomIn);
	tbtn->setFocusPolicy(Qt::NoFocus);
	tblayout->addWidget(tbtn);
	m_btnZoomIn = tbtn;

	tbtn = new QToolButton;
	tbtn->setDefaultAction(m_mainS->actions()->viewZoomOut);
	tbtn->setFocusPolicy(Qt::NoFocus);
	tblayout->addWidget(tbtn);
	m_btnZoomOut = tbtn;

	tbtn = new QToolButton;
	tbtn->setDefaultAction(m_mainS->actions()->viewZoomFull);
	tbtn->setFocusPolicy(Qt::NoFocus);
	tblayout->addWidget(tbtn);
	connect(m_mainS->actions()->viewZoomFull, SIGNAL(triggered()), this, SLOT(zoomFull()));
	tblayout->addItem(new QSpacerItem(10, 10, QSizePolicy::Expanding, QSizePolicy::Fixed));
	m_btnZoomFull = tbtn;

	m_buttons->setLayout(tblayout);

	m_scrollbar = new QScrollBar(Qt::Horizontal, this);
	m_scrollbar->setSingleStep(EAD_SAMPLES_PER_SECOND);
	connect(m_scrollbar, SIGNAL(valueChanged(int)), this, SLOT(setSampleOffset(int)));

	m_buttons->setCursor(Qt::ArrowCursor);
	m_scrollbar->setCursor(Qt::ArrowCursor);

	// Set fixed size for m_lblSecondsPerDivision
	int nWidth = 0;
	for (int i = 0; ChartScope::anSecondsPerDivision[i] > 0; i++)
	{
		QString s = timestampString(ChartScope::anSecondsPerDivision[i]);
		nWidth = qMax(nWidth, m_lblSecondsPerDivision->fontMetrics().width(s));
	}
	m_lblSecondsPerDivision->setMinimumWidth(nWidth);

	m_lblRecording = new QLabel(tr("RECORDING"), this);
	m_lblRecording->setStyleSheet(
		//"background-color: rgba(255, 127, 127, 127);"
		"background-color: #fdd;"
		"border-style: outset;"
		"border-width: 1px;"
		"border-color: #500;"
		//"border-radius: 7px;"
		"font-weight: bold;"
		"padding: 5px 10px;");
	m_lblRecording->setVisible(false);
}

void ChartWidget::setStatusBar(QStatusBar* statusbar)
{
	if (m_lblStatus != NULL)
	{
		delete m_lblStatus;
		m_lblStatus = NULL;
	}

	m_statusbar = statusbar;

	if (m_statusbar != NULL)
	{
		m_lblStatus = new QLabel();
		m_statusbar->insertPermanentWidget(0, m_lblStatus);
		connect(m_chartS, SIGNAL(statusTextChanged(QString)), m_lblStatus, SLOT(setText(QString)));
	}
}

void ChartWidget::repaintChart()
{
	m_bRedraw = true;
	update();
	//qDebug() << "repaintChart:" << QTime::currentTime();
}

void ChartWidget::updateRecordings()
{
	if (m_mainS->viewType() == EadView_Recording)
	{
		if (!m_timerUpdate->isActive())
		{
			repaintChart();
			// REFACTOR: adjust timing so that fewer updates are done when we're zoomed out
			//  enough that many data samples are drawn in a single pixel
			// Update 10 times per second
			m_timerUpdate->start(100);
			m_nRecordingUpdates = 0;
		}
		else
			m_nRecordingUpdates++;
	}
}

void ChartWidget::on_timerUpdate_timeout()
{
	if (m_nRecordingUpdates > 0)
	{
		m_nRecordingUpdates = 0;
		repaintChart();
	}
	else
	{
		m_timerUpdate->stop();
	}
}

QSize ChartWidget::calcPixmapSize() const
{
	QRect rcPixmapMax(20, m_buttons->height(), width() - 35, height() - m_buttons->height() - 20);

	QSize sz = m_rcPixmapMax.size();
	int nCols = 0;
	if (m_mainS->taskType() == EadTask_Publish)
	{
		if (Globals->publisherSettings()->bPublishCols)
			nCols = Globals->publisherSettings()->nPublishCols;
		if (Globals->publisherSettings()->bPublishHeight && Globals->publisherSettings()->nPublishSize > 0)
		{
			sz = QSize(Globals->publisherSettings()->nPublishSize, Globals->publisherSettings()->nPublishSize);
		}
	}

	sz = m_pixmap->sizeForAvailableArea(sz, 0);
	return sz;
}

void ChartWidget::resizeEvent(QResizeEvent* e)
{
	Q_UNUSED(e);
	
	m_bRedraw = true;

	m_rcPixmapMax = QRect(20, m_buttons->height(), width() - 35, height() - m_buttons->height() - 20);
	QSize sz = calcPixmapSize();
	
	QRect rcScroll(m_rcPixmapMax.left(), m_rcPixmapMax.top() + sz.height(), sz.width(), 20);
	
	m_scrollbar->setGeometry(rcScroll);

	m_lblRecording->move(width() - m_lblRecording->width() - 20, 5);
}

void ChartWidget::paintEvent(QPaintEvent* e)
{
	Q_UNUSED(e);

	QPainter painter(this);

	painter.fillRect(rect(), Qt::white);

	if (m_bRedraw)
	{
		m_bRedraw = false;
		/*
		//int n = Globals->publisherSettings()->nPublisherPercentSize;
		int nCols = 0;
		QSize sz = calcPixmapSize();
		if (m_mainS->taskType() == EadTask_Publish)
		{
			if (Globals->publisherSettings()->bPublishCols)
				nCols = Globals->publisherSettings()->nPublishCols;
		}
		
		PublisherSettings* pub = Globals->publisherSettings();
		bool bOverrideTimebase = (m_mainS->taskType() == EadTask_Publish && pub->bPublishTimebase);

		ChartPixmapParams params;
		params.file = m_chartS->file();
		params.view = m_chartS->view();
		params.task = m_mainS->taskType();
		params.peakMode = m_mainS->peakMode();
		params.nPeakModeRecId = m_scope->peakModeRecId();
		params.nCols = nCols;
		params.size = sz;
		params.nSampleOffset = m_chartS->sampleOffset();
		params.vwiHilight = m_vwiHilight;
		if (m_task == EadTask_Publish)
		{
			params.elements = pub->publisherChartElements;
		}
		// Check whether we need to hide the wave comments
		else
		{
			if (!Globals->viewSettings()->bShowWaveComments)
				params.elements &= ~ChartElement_WaveComments;
		}
		if (bOverrideTimebase)
		{
			params.nSecondsPerDivision = pub->nPublishTimebase * 60;
			m_chartS->secondsPerDivision() = params.nSecondsPerDivision;
		}
		else
		{
			m_chartS->secondsPerDivision() = ChartScope::anSecondsPerDivision[m_iSecondsPerDivision];
		}
		params.nSecondsPerDivision = m_chartS->secondsPerDivision();
		m_pixmap->draw(params);

		m_btnZoomIn->setEnabled(!bOverrideTimebase);
		m_btnZoomOut->setEnabled(!bOverrideTimebase);
		m_btnZoomFull->setEnabled(!bOverrideTimebase);
		m_lblSecondsPerDivision->setText(timebaseString());
		*/
		QSize sz = calcPixmapSize();
		m_chartS->draw(sz);

		int nSamplesPerPage = int(m_chartS->secondsPerDivision() * EAD_SAMPLES_PER_SECOND * m_pixmap->params().nCols);
		m_scrollbar->setPageStep(nSamplesPerPage);
		m_scrollbar->setSingleStep(nSamplesPerPage / m_pixmap->params().nCols);

		// Total number of samples in the dataset
		int nSamples = m_chartS->sampleCount();

		int nMaxScroll = nSamples - nSamplesPerPage;
		if (nMaxScroll < 0)
			nMaxScroll = m_chartS->sampleOffset();
		m_scrollbar->setMaximum(nMaxScroll);
		m_scrollbar->setValue(m_chartS->sampleOffset());

		QRect rcScroll(m_rcPixmapMax.left(), m_rcPixmapMax.top() + sz.height(), sz.width(), 20);
		if (rcScroll != m_scrollbar->geometry())
			m_scrollbar->setGeometry(rcScroll);
	}

	m_rcPixmap = m_pixmap->borderRect();
	m_rcPixmap.moveTo(m_rcPixmapMax.topLeft());
	painter.drawPixmap(m_rcPixmap.topLeft(), m_pixmap->pixmap());

	// If the user is currently selecting an area to zoom in on:
	if (m_bDragging && m_bSelecting)
	{
		int x0 = m_ptClickWidget.x();
		int x1 = m_ptMouseWidget.x();
		if (x0 > x1)
			qSwap(x0, x1);
		x0 = qMax(x0, m_rcPixmap.left());
		x1 = qMin(x1, m_rcPixmap.right());
		
		QRect rcBorder = m_rcPixmap;
		QRect rc(QPoint(x0, rcBorder.top() + 1), QPoint(x1, rcBorder.bottom() - 1));
		QColor clr(0, 0, 255, 100);
		painter.fillRect(rc, clr);
	}
}

void ChartWidget::leaveEvent(QEvent* e)
{
	Q_UNUSED(e);
	m_chartS->setHilight(NULL);
}

void ChartWidget::mousePressEvent(QMouseEvent* e)
{
	QRect rcWaveforms = m_rcPixmap;

	ChartPointInfo info;
	QPoint ptPixmap = e->pos() - m_rcPixmap.topLeft();
	m_pixmap->fillChartPointInfo(ptPixmap, &info);
	m_clickInfo = info;
	ViewWaveInfo* vwi = info.vwi;

	m_ptMouseWidget = e->pos();
	m_ptMousePixmap = ptPixmap;
	m_ptClickWidget = m_ptMouseWidget;
	m_ptClickPixmap = m_ptMousePixmap;
	
	const WaveInfo* wave = NULL;
	if (vwi != NULL)
		wave = vwi->wave();

	if (e->button() == Qt::LeftButton)
	{
		// Clicked on a chosen peak:
		if (info.iChosenPeak >= 0)
		{
			//vwi->unchoosePeakAtDidx(info.didxChosenPeak);
			m_bDragging = true;
		}
		// Clicked on a detected peak:
		else if (info.didxPossiblePeak >= 0)
		{
			vwi->choosePeakAtDidx(info.didxPossiblePeak);
		}
		// Clicked on a peak area handle:
		else if (info.iLeftAreaHandle >= 0 || info.iRightAreaHandle >= 0)
		{
			m_bDragging = true;
		}
		// Clicked on a wave:
		else if (wave != NULL)
		{
			// If the user Ctrl+clicks on the selected FID wave while in peak editing mode:
			if (e->modifiers() == Qt::ControlModifier && m_chartS->params().peakMode == EadPeakMode_Edit && wave->recId() == m_chartS->params().nPeakModeRecId)
			{
				addPeak(vwi, m_ptClickPixmap.x());
			}
			else
			{
				m_bDragging = true;
				// REFACTOR: remove m_waveDrag and use m_clickInfo instead
				//m_waveDrag = vwi->waveInfo();
				m_nDragOrigDivisionOffset = vwi->divisionOffset();
			}
		}
		else if (rcWaveforms.contains(e->pos()))
		{
			m_bDragging = true;
			m_bSelecting = true;
			updateStatus();
			update();
		}
		else
		{
			m_bDragging = false;
		}
	}
	/*
	else if (e->button() == Qt::RightButton)
	{
		if (wave != NULL)
		{
			int iMouse = m_pixmap->xToCenterSample(wave, e->pos().x());
			int i0 = qMax(iMouse - 25, 0);
			int i1 = qMin(iMouse + 25, wave->display.size() - 1);
			
			for (int i = i0; i <= i1; i++)
			{
				
			}
		}
	}
	*/

	updateStatus();
}

void ChartWidget::mouseReleaseEvent(QMouseEvent* e)
{
	Q_UNUSED(e);

	m_ptMouseWidget = e->pos();
	m_ptMousePixmap = m_ptMouseWidget - m_rcPixmap.topLeft();

	if (m_bDragging)
	{
		const ChartPointInfo& info = m_clickInfo;
		ViewWaveInfo* vwi = info.vwi;

		if (m_bSelecting)
		{
			int dx = qAbs(m_ptClickPixmap.x() - m_ptMousePixmap.x()) + 1;

			// Require minimal zoom width to avoid acting on mistaken mouse clicks
			if (dx > 10)
			{
				int iSampleStart = m_pixmap->xToSampleOffset(m_ptClickPixmap.x());
				int iSampleEnd = m_pixmap->xToSampleOffset(m_ptMousePixmap.x());
				int iSampleCenter = (iSampleStart + iSampleEnd) / 2;
				int nSamples = qAbs(iSampleEnd - iSampleStart);
				double nSeconds = double(nSamples) / EAD_SAMPLES_PER_SECOND;
				
				// New seconds per division
				double nSecondsPerDivision = nSeconds / 10;
				m_chartS->setSecondsPerDivisionMin(nSecondsPerDivision);
				m_chartS->center(iSampleCenter);
			}

			update();
		}
		// If we were dragging a wave
		else if (vwi != NULL && info.iLeftAreaHandle < 0 && info.iRightAreaHandle < 0)
		{
			vwi->bAssureVisibility = true; // HACK: tell ChartPixmap to reposition this wave for visibility if necessary
		}
	}
	m_bDragging = false;
	m_bSelecting = false;
	//m_waveDrag = NULL;
	
	updateStatus();
}

void ChartWidget::mouseMoveEvent(QMouseEvent* e)
{
	m_ptMouseWidget = e->pos();
	m_ptMousePixmap = m_ptMouseWidget - m_rcPixmap.topLeft();

	if (m_bDragging)
	{
		const ChartPointInfo& info = m_clickInfo;
		ViewWaveInfo* vwi = info.vwi;
		WaveInfo* wave = (vwi != NULL) ? vwi->waveInfo() : NULL;

		if (m_bSelecting)
		{
			// While selecting, don't let the mouse move out of the pixmap
			if (m_ptMousePixmap.x() < 0)
				m_ptMousePixmap.setX(0);
			else if (m_ptMousePixmap.x() >= m_rcPixmap.width())
				m_ptMousePixmap.setX(m_rcPixmap.width() - 1);
			update();
		}
		else if (info.iLeftAreaHandle >= 0)
		{
			int x = e->pos().x() - m_rcPixmap.left();
			int i = m_pixmap->xToCenterSample(info.vwi->wave(), x);
			info.vwi->waveInfo()->peaksChosen[info.iLeftAreaHandle].didxLeft = i;
			info.vwi->waveInfo()->calcPeakArea(info.iLeftAreaHandle);
			info.vwi->waveInfo()->calcAreaPercents();
			repaintChart();
		}
		else if (info.iRightAreaHandle >= 0)
		{
			int x = e->pos().x() - m_rcPixmap.left();
			int i = m_pixmap->xToCenterSample(info.vwi->wave(), x);
			info.vwi->waveInfo()->peaksChosen[info.iRightAreaHandle].didxRight = i;
			info.vwi->waveInfo()->calcPeakArea(info.iRightAreaHandle);
			info.vwi->waveInfo()->calcAreaPercents();
			repaintChart();
		}
		else if (info.iChosenPeak >= 0)
		{
			WavePeakChosenInfo& marker = wave->peaksChosen[info.iChosenPeak];
			int x = e->pos().x() - m_rcPixmap.left();
			int didx = m_pixmap->xToCenterSample(info.vwi->wave(), x);
			int d = didx - marker.didxMiddle;
			marker.didxLeft += d;
			marker.didxMiddle += d;
			marker.didxRight += d;
			info.vwi->waveInfo()->calcPeakArea(info.iRightAreaHandle);
			info.vwi->waveInfo()->calcAreaPercents();
			repaintChart();
		}
		else if (wave != NULL)
		{
			if (e->modifiers() == Qt::ControlModifier && m_chartS->params().peakMode == EadPeakMode_Edit && wave == m_pixmap->waveOfPeaks())
			{
				setCursor(Qt::PointingHandCursor);
			}
			else
			{
				// How much has the mouse been moved in total during this drag?
				double nStart = m_pixmap->yToValue(vwi, m_ptClickPixmap.y());
				double nEnd = m_pixmap->yToValue(vwi, m_ptMousePixmap.y());
				double nVolts = nEnd - nStart;
				double nOffset = nVolts / vwi->voltsPerDivision();
				// Set the channel offset appropriately
				vwi->setDivisionOffset(m_nDragOrigDivisionOffset - nOffset);

				repaintChart();
			}
		}
	}
	else
	{
		ChartPointInfo info;
		QPoint ptPixmap = e->pos() - m_rcPixmap.topLeft();
		m_pixmap->fillChartPointInfo(ptPixmap, &info);
		m_chartS->setHilight(info.vwi);
		
		if (info.didxPossiblePeak >= 0 || info.iChosenPeak >= 0 || info.iLeftAreaHandle >= 0 || info.iRightAreaHandle >= 0)
		{
			setCursor(Qt::PointingHandCursor);
		}
		else if (info.vwi != NULL)
		{
			Qt::CursorShape shape = Qt::OpenHandCursor;

			if (e->modifiers() == Qt::ControlModifier && m_chartS->params().peakMode == EadPeakMode_Edit && info.vwi->wave() == m_pixmap->waveOfPeaks())
				shape = Qt::PointingHandCursor;

			setCursor(shape);
		}
		else if (m_rcPixmap.contains(e->pos()))
		{
			setCursor(Qt::CrossCursor);
		}
		else
			unsetCursor();
	}

	updateStatus();
}

void ChartWidget::mouseDoubleClickEvent(QMouseEvent* e)
{
	QRect rcWaveforms = m_rcPixmap;

	ChartPointInfo info;
	QPoint ptPixmap = e->pos() - m_rcPixmap.topLeft();
	m_pixmap->fillChartPointInfo(ptPixmap, &info);
	m_clickInfo = info;
	ViewWaveInfo* vwi = info.vwi;

	m_ptMouseWidget = e->pos();
	m_ptMousePixmap = ptPixmap;
	m_ptClickWidget = m_ptMouseWidget;
	m_ptClickPixmap = m_ptMousePixmap;

	const WaveInfo* wave = NULL;
	if (vwi != NULL)
		wave = vwi->wave();

	if (e->button() == Qt::LeftButton)
	{
		// Clicked on a chosen peak:
		if (info.iChosenPeak >= 0)
		{
		}
		// Clicked on a detected peak:
		else if (info.didxPossiblePeak >= 0)
		{
		}
		// Clicked on a peak area handle:
		else if (info.iLeftAreaHandle >= 0 || info.iRightAreaHandle >= 0)
		{
		}
		// Clicked on a wave:
		else if (wave != NULL)
		{
			QPoint ptGlobal = mapToGlobal(e->pos());
			openWaveEditorDialog(vwi, ptGlobal);
		}
		else if (rcWaveforms.contains(e->pos()))
		{
		}
		else
		{
			m_bDragging = false;
		}
	}

	updateStatus();
}

void ChartWidget::wheelEvent(QWheelEvent* e)
{
	// Each 'click' of the scroll wheel produces a delta of 120
	int nDivs = e->delta() / 120;
	int nSamples = -nDivs * m_chartS->secondsPerDivision() * EAD_SAMPLES_PER_SECOND;
	int i = m_scrollbar->value() + nSamples;
	if (i < 0)
		i = 0;
	m_scrollbar->setValue(i);
}

void ChartWidget::contextMenuEvent(QContextMenuEvent* e)
{
	QRect rcWaveforms = m_rcPixmap;

	ChartPointInfo info;
	QPoint ptPixmap = e->pos() - m_rcPixmap.topLeft();
	m_pixmap->fillChartPointInfo(ptPixmap, &info);
	m_clickInfo = info;
	ViewWaveInfo* vwi = info.vwi;

	m_ptMouseWidget = e->pos();
	m_ptMousePixmap = ptPixmap;
	m_ptClickWidget = m_ptMouseWidget;
	m_ptClickPixmap = m_ptMousePixmap;

	const WaveInfo* wave = NULL;
	if (vwi != NULL)
		wave = vwi->wave();

	QMenu menu(this);
	QAction* actSettings = NULL;
	QAction* actAddMarker = NULL;
	QAction* actRemoveMarker = NULL;
	{
		// Clicked on a chosen peak:
		if (info.iChosenPeak >= 0)
		{
			actRemoveMarker = new QAction(tr("Remove Marker"), &menu);
			menu.addAction(actRemoveMarker);
		}
		// Clicked on a detected peak:
		else if (info.didxPossiblePeak >= 0)
		{
			actAddMarker = new QAction(tr("Add Peak Marker"), &menu);
			menu.addAction(actAddMarker);
		}
		// Clicked on a peak area handle:
		else if (info.iLeftAreaHandle >= 0 || info.iRightAreaHandle >= 0)
		{
			actRemoveMarker = new QAction(tr("Remove Marker"), &menu);
			menu.addAction(actRemoveMarker);
		}
		// Clicked on a wave:
		else if (wave != NULL)
		{
			actSettings = new QAction(tr("Settings..."), &menu);
			menu.addAction(actSettings);
			if (wave->type == WaveType_FID)
				actAddMarker = new QAction(tr("Add Peak Marker"), &menu);
			else
				actAddMarker = new QAction(tr("Add Time Marker"), &menu);
			menu.addAction(actAddMarker);
		}
		else if (rcWaveforms.contains(e->pos()))
		{
			menu.addAction(m_mainS->actions()->viewZoomIn);
			menu.addAction(m_mainS->actions()->viewZoomOut);
			menu.addAction(m_mainS->actions()->viewZoomFull);
		}
		else
		{
		}
	}
	if (menu.actions().size() > 0)
	{
		QPoint ptGlobal = mapToGlobal(e->pos());
		QAction* act = menu.exec(ptGlobal);
		if (act == NULL)
			;
		else if (act == actSettings)
		{
			openWaveEditorDialog(vwi, ptGlobal);
		}
		else if (act == actAddMarker)
		{
			if (info.didxPossiblePeak >= 0)
				vwi->choosePeakAtDidx(info.didxPossiblePeak);
			else
				addPeak(vwi, ptPixmap.x());
		}
		else if (act == actRemoveMarker)
		{
			CHECK_ASSERT_NORET(info.iChosenPeak >= 0);
			if (info.iChosenPeak >= 0)
				vwi->unchoosePeakAtIndex(info.iChosenPeak);
		}
	}

	e->accept();
	updateStatus();
}

void ChartWidget::addPeak(ViewWaveInfo* vwi, int x)
{
	int didx1 = m_pixmap->xToCenterSample(vwi->wave(), x);
	int didx0 = m_pixmap->xToCenterSample(vwi->wave(), x - 10);
	int didx2 = m_pixmap->xToCenterSample(vwi->wave(), x + 10);

	if (didx0 >= 0 && didx2 < vwi->wave()->display.size())
	{
		WavePeakChosenInfo peak;
		peak.didxLeft = didx0;
		peak.didxMiddle = didx1;
		peak.didxRight = didx2;
		vwi->choosePeak(peak);
	}
}

void ChartWidget::openWaveEditorDialog(ViewWaveInfo* vwi, const QPoint& ptGlobal)
{
	WaveEditorDialog editor(vwi, vwi->editorFlags, this);
	editor.move(ptGlobal);
	editor.exec();
}

void ChartWidget::updateStatus()
{
	if (m_statusbar == NULL)
		return;

	const ChartPixmap* pixmap = m_chartS->pixmap();

	QString s;
	if (m_bDragging && m_bSelecting)
	{
		int iSampleStart = pixmap->xToSampleOffset(m_ptClickPixmap.x());
		int iSampleEnd = pixmap->xToSampleOffset(m_ptMousePixmap.x());
		m_chartS->setSelectionRange(iSampleStart, iSampleEnd);
	}
	else
	{
		int iSample = pixmap->xToSampleOffset(m_ptMousePixmap.x());
		m_chartS->setMousePosition(iSample);
	}
}
