/**
 * Copyright (C) 2008,2009  Ellis Whitehead
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

#include "IdacControlThread.h"

#include <QtDebug>
#include <QMutex>

#include <Check.h>

#include "IdacDriverManager.h"


IdacControlThread::IdacControlThread(QObject* parent)
	: QThread(parent)
{
	m_manager = NULL;
}

IdacControlThread::~IdacControlThread()
{
	delete m_manager;
}

void IdacControlThread::run()
{
	m_manager = new IdacDriverManager();
	exec();
}