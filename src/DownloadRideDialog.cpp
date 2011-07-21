/*
 * $Id: DownloadRideDialog.cpp,v 1.4 2006/08/11 20:02:13 srhea Exp $
 *
 * Copyright (c) 2006-2008 Sean C. Rhea (srhea@srhea.net)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "DownloadRideDialog.h"
#include "Device.h"
#include "MainWindow.h"
#include <assert.h>
#include <errno.h>
#include <QtGui>
#include <boost/bind.hpp>
#include <boost/foreach.hpp>
#include <stdio.h>

DownloadRideDialog::DownloadRideDialog(MainWindow *mainWindow,
                                       const QDir &home) :
    mainWindow(mainWindow), home(home), cancelled(false),
    action(actionIdle)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle("Download Ride Data");

    portCombo = new QComboBox(this);


    statusLabel = new QLabel(this);
    statusLabel->setIndent(10);
    statusLabel->setTextFormat(Qt::PlainText);
    // XXX: make statusLabel scrollable

    // would prefer a progress bar, but some devices (eg. PTap) don't give
    // a hint about the total work, so this isn't possible.
    progressLabel = new QLabel(this);
    progressLabel->setIndent(10);
    progressLabel->setTextFormat(Qt::PlainText);

    deviceCombo = new QComboBox(this);
    QList<QString> deviceTypes = Devices::typeNames();
    assert(deviceTypes.size() > 0);
    BOOST_FOREACH(QString device, deviceTypes) {
        deviceCombo->addItem(device);
    }
    connect(deviceCombo, SIGNAL(currentIndexChanged(QString)), this, SLOT(deviceChanged(QString)));

    downloadButton = new QPushButton(tr("&Download"), this);
    eraseRideButton = new QPushButton(tr("&Erase Ride(s)"), this);
    rescanButton = new QPushButton(tr("&Rescan"), this);
    cancelButton = new QPushButton(tr("&Cancel"), this);
    closeButton = new QPushButton(tr("&Close"), this);

    downloadButton->setDefault( true );

    connect(downloadButton, SIGNAL(clicked()), this, SLOT(downloadClicked()));
    connect(eraseRideButton, SIGNAL(clicked()), this, SLOT(eraseClicked()));
    connect(rescanButton, SIGNAL(clicked()), this, SLOT(scanCommPorts()));
    connect(cancelButton, SIGNAL(clicked()), this, SLOT(cancelClicked()));
    connect(closeButton, SIGNAL(clicked()), this, SLOT(closeClicked()));

    QHBoxLayout *buttonLayout = new QHBoxLayout;
    buttonLayout->addWidget(downloadButton);
    buttonLayout->addWidget(eraseRideButton);
    buttonLayout->addWidget(rescanButton);
    buttonLayout->addWidget(cancelButton);
    buttonLayout->addWidget(closeButton);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(new QLabel(tr("Select device type:"), this));
    mainLayout->addWidget(deviceCombo);
    mainLayout->addWidget(new QLabel(tr("Select port:"), this));
    mainLayout->addWidget(portCombo);
    mainLayout->addWidget(new QLabel(tr("Info:"), this));
    mainLayout->addWidget(statusLabel, 1);
    mainLayout->addWidget(progressLabel);
    mainLayout->addLayout(buttonLayout);

    scanCommPorts();
}

void
DownloadRideDialog::setReadyInstruct()
{
    progressLabel->setText("");

    if (portCombo->count() == 0) {
        statusLabel->setText(tr("No devices found.  Make sure the device\n"
                          "unit is plugged into the computer,\n"
                          "then click \"Rescan\" to check again."));
        updateAction( actionMissing );
    }
    else {
        DevicesPtr devtype = Devices::getType(deviceCombo->currentText());
        QString inst = devtype->downloadInstructions();
        if (inst.size() == 0)
            statusLabel->setText("Click Download to begin downloading.");
        else
            statusLabel->setText(inst + ", \nthen click Download.");

        updateAction( actionIdle );
    }
}

void
DownloadRideDialog::scanCommPorts()
{
    portCombo->clear();
    QString err;
    devList = CommPort::listCommPorts(err);
    if (err != "") {
        QString msg = "Warning(s):\n\n" + err + "\n\nYou may need to (re)install "
            "the FTDI or PL2303 drivers before downloading.";
        QMessageBox::warning(0, "Error Loading Device Drivers", msg,
                             QMessageBox::Ok, QMessageBox::NoButton);
    }
    for (int i = 0; i < devList.size(); ++i) {
        portCombo->addItem(devList[i]->id());
        // XXX Hack: SRM PCV download cables use the PL2203 chipset.  If the
        // first device name contains "PL2303", then, we're probably dealing
        // with an SRM, so go ahead and select the SRM device.  Generalize?
        if ((i == 0) && devList[i]->name().contains("PL2303")) {
            int j = deviceCombo->findText("SRM");
            if (j >= 0)
                deviceCombo->setCurrentIndex(j);
        }
    }
    if (portCombo->count() > 0)
        downloadButton->setFocus();
    setReadyInstruct();
}

bool
DownloadRideDialog::isCancelled()
{
    return cancelled;
}

void
DownloadRideDialog::updateAction( downloadAction newAction )
{

    switch( newAction ){
      case actionMissing:
        downloadButton->setEnabled(false);
        eraseRideButton->setEnabled(false);
        rescanButton->setEnabled(true);
        cancelButton->setEnabled(false);
        closeButton->setEnabled(true);
        portCombo->setEnabled(false);
        deviceCombo->setEnabled(true);
        break;

      case actionIdle: {
        DevicesPtr devtype = Devices::getType(deviceCombo->currentText());

        downloadButton->setEnabled(true);
        eraseRideButton->setEnabled(devtype->canCleanup());
        rescanButton->setEnabled(true);
        cancelButton->setEnabled(false);
        closeButton->setEnabled(true);
        portCombo->setEnabled(true);
        deviceCombo->setEnabled(true);
        break;
        }

      case actionDownload:
      case actionCleaning:
        downloadButton->setEnabled(false);
        eraseRideButton->setEnabled(false);
        rescanButton->setEnabled(false);
        cancelButton->setEnabled(true);
        closeButton->setEnabled(false);
        portCombo->setEnabled(false);
        deviceCombo->setEnabled(false);
        break;

    }

    action = newAction;
    cancelled = false;
    QCoreApplication::processEvents();
}

void
DownloadRideDialog::updateStatus(const QString &statusText)
{
    statusLabel->setText(statusLabel->text() + "\n" + statusText);
    QCoreApplication::processEvents();
}

void
DownloadRideDialog::updateProgress( const QString &progressText )
{
    progressLabel->setText(progressText);
    QCoreApplication::processEvents();
}


void
DownloadRideDialog::deviceChanged( QString deviceType )
{
    (void)deviceType;

    updateAction(action);
    setReadyInstruct();
}

void
DownloadRideDialog::downloadClicked()
{
    updateAction( actionDownload );

    updateProgress( "" );
    statusLabel->setText( "" );

    CommPortPtr dev;
    for (int i = 0; i < devList.size(); ++i) {
        if (devList[i]->id() == portCombo->currentText()) {
            dev = devList[i];
            break;
        }
    }
    assert(dev);
    QString err;
    QList<DeviceDownloadFile> files;

    DevicesPtr devtype = Devices::getType(deviceCombo->currentText());
    DevicePtr device = devtype->newDevice( dev );

    if( ! device->preview(
        boost::bind(&DownloadRideDialog::updateStatus, this, _1),
        err ) ){

        QMessageBox::information(this, tr("Preview failed"), err);
        updateAction( actionIdle );
        return;
    }

    QList<DeviceRideItemPtr> &rides( device->rides() );
    if( ! rides.empty() ){
        // XXX: let user select, which rides he wants to download
        for( int i = 0; i < rides.size(); ++i ){
            rides.at(i)->wanted = true;
        }
    }

    if (!device->download( home, files,
            boost::bind(&DownloadRideDialog::isCancelled, this),
            boost::bind(&DownloadRideDialog::updateStatus, this, _1),
            boost::bind(&DownloadRideDialog::updateProgress, this, _1),
            err))
    {
        if (cancelled) {
            QMessageBox::information(this, tr("Download canceled"),
                                     tr("Cancel clicked by user."));
            cancelled = false;
        }
        else {
            QMessageBox::information(this, tr("Download failed"), err);
        }
        updateStatus(tr("Download failed"));
        updateAction( actionIdle );
        return;
    }

    updateProgress( "" );

    int failures = 0;
    for( int i = 0; i < files.size(); ++i ){
        QString filename( files.at(i).startTime
            .toString("yyyy_MM_dd_hh_mm_ss")
            + "." + files.at(i).extension );
        QString filepath( home.absoluteFilePath(filename) );

        if (QFile::exists(filepath)) {
            if (QMessageBox::warning( this,
                    tr("Ride Already Downloaded"),
                    tr("The ride starting at %1 appears to have already "
                        "been downloaded.  Do you want to overwrite the "
                        "previous download?")
                        .arg(files.at(i).startTime.toString()),
                    tr("&Overwrite"), tr("&Skip"),
                    QString(), 1, 1) == 1) {
                QFile::remove(files.at(i).name);
                updateStatus(tr("skipped file %1")
                    .arg( files.at(i).name ));
                continue;
            }
        }

#ifdef __WIN32__
        // Windows ::rename won't overwrite an existing file.
        if (QFile::exists(filepath)) {
            QFile old(filepath);
            if (!old.remove()) {
                QMessageBox::critical(this, tr("Error"),
                    tr("Failed to remove existing file %1: %2")
                        .arg(filepath)
                        .arg(old.error()) );
                QFile::remove(files.at(i).name);
                updateStatus(tr("failed to rename %1 to %2")
                    .arg( files.at(i).name )
                    .arg( filename ));
                ++failures;
                continue;
            }
        }
#endif

        // Use ::rename() instead of QFile::rename() to get forced overwrite.
        if (rename(QFile::encodeName(files.at(i).name),
            QFile::encodeName(filepath)) < 0) {

            QMessageBox::critical(this, tr("Error"),
                tr("Failed to rename %1 to %2: %3")
                    .arg(files.at(i).name)
                    .arg(filepath)
                    .arg(strerror(errno)) );
                updateStatus(tr("failed to rename %1 to %2")
                    .arg( files.at(i).name )
                    .arg( filename ));
            QFile::remove(files.at(i).name);
            ++failures;
            continue;
        }

        QFile::remove(files.at(i).name);
        mainWindow->addRide(filename);
    }

    if( ! failures )
        updateStatus( tr("download completed successfully") );

    updateAction( actionIdle );
}

void
DownloadRideDialog::eraseClicked()
{
    updateAction( actionCleaning );

    statusLabel->setText( "" );
    updateProgress( "" );

    CommPortPtr dev;
    for (int i = 0; i < devList.size(); ++i) {
        if (devList[i]->id() == portCombo->currentText()) {
            dev = devList[i];
            break;
        }
    }
    assert(dev);
    DevicesPtr devtype = Devices::getType(deviceCombo->currentText());
    DevicePtr device = devtype->newDevice( dev );

    QString err;
    if( device->cleanup( err) )
        updateStatus( tr("cleaned data") );
    else
        updateStatus( err );

    updateAction( actionIdle );
}

void
DownloadRideDialog::cancelClicked()
{
    switch( action ){
      case actionIdle:
      case actionMissing:
        // do nothing
        break;

      default:
        cancelled = true;
        break;
     }
}

void
DownloadRideDialog::closeClicked()
{
    accept();
}


