/* Copyright (c) 2026
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#pragma once

#include <QDialog>

#include "ui_DesktopLinkDialog.h"

namespace QGBA {

class DesktopLinkDialog : public QDialog {
Q_OBJECT

public:
	DesktopLinkDialog(const QString& initialRomPath = {}, const QString& initialName = {}, QWidget* parent = nullptr);

private slots:
	void browseRom();
	void browseIcon();
	void browseDestination();
	void updatePreview();
	void validate();
	void createLink();

private:
	QString m_lastAutoName;
	Ui::DesktopLinkDialog m_ui;
};

}
