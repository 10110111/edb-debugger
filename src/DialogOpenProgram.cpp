/*
Copyright (C) 2015 Ruslan Kabatsayev <b7.10110111@gmail.com>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "DialogOpenProgram.h"
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QGridLayout>
#include <QComboBox>
#include <QDebug>
#include "edb.h"

static void warnTheUser()
{
	qWarning() << QObject::tr("Failed to setup program arguments and working directory entries for file open dialog, please report and be sure to tell your Qt version");
}

DialogOpenProgram::DialogOpenProgram(QWidget* parent,const QString& caption, const QString& directory,const QString& filter)
	: QFileDialog(parent,caption,directory,filter),
			  argsEdit(new QLineEdit(this)),
			  workDir(new QLineEdit(QDir::currentPath(),this))
{
	// We want to be sure that the layout is as we expect it
	QGridLayout* const layout=dynamic_cast<QGridLayout*>(this->layout());
	if(!layout || layout->rowCount()!=4 || layout->columnCount()!=3) { warnTheUser(); return; }

	// File type filter is useless for our purposes, so hide it
	const auto filterComboItem=layout->itemAtPosition(3,1);
	const auto filterLabelItem=layout->itemAtPosition(3,0);
	if(!filterComboItem || !filterLabelItem) { warnTheUser(); return; }
	const auto filterCombo=dynamic_cast<QComboBox*>(filterComboItem->widget());
	const auto filterLabel=dynamic_cast<QLabel*>(filterLabelItem->widget());
	if(!filterCombo || !filterLabel) { warnTheUser(); return; }
	filterCombo->hide();
	filterLabel->hide();

	setFileMode(QFileDialog::ExistingFile);
	const int rowCount=layout->rowCount();
	QPushButton* const browseDirButton(new QPushButton(tr("&Browse..."),this));
	const auto argsLabel=new QLabel(tr("Program &arguments:"),this);
	argsLabel->setBuddy(argsEdit);
	layout->addWidget(argsLabel,rowCount-1,0);
	layout->addWidget(argsEdit,rowCount-1,1);
	const auto workDirLabel=new QLabel(tr("Working &directory:"),this);
	workDirLabel->setBuddy(workDir);
	layout->addWidget(workDirLabel,rowCount,0);
	layout->addWidget(workDir,rowCount,1);
	layout->addWidget(browseDirButton,rowCount,2);

	connect(browseDirButton,SIGNAL(clicked()),this,SLOT(browsePressed()));
}

void DialogOpenProgram::browsePressed()
{
	const QString dir=QFileDialog::getExistingDirectory(this,tr("Choose program working directory"),workDir->text());
	if(dir.size()) workDir->setText(dir);
}

QList<QByteArray> DialogOpenProgram::arguments() const
{
	const QStringList args=edb::v1::parse_command_line(argsEdit->text());
	QList<QByteArray> ret;
	for(auto arg : args)
		ret << arg.toLocal8Bit();
	return ret;
}

QString DialogOpenProgram::workingDirectory() const
{
	return workDir->text();
}
