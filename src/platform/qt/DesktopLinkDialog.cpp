/* Copyright (c) 2026
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "DesktopLinkDialog.h"
#include "moc_DesktopLinkDialog.cpp"

#include "GBAApp.h"
#include "utils.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QMessageBox>
#include <QPixmap>
#include <QProcess>
#include <QPushButton>
#include <QRegularExpression>
#include <QStandardPaths>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shlobj.h>
#include <objbase.h>
#endif

using namespace QGBA;

namespace {

QString sanitizeFileName(const QString& name) {
	QString sanitized = name.trimmed();
	sanitized.replace(QRegularExpression("[\\\\/:*?\"<>|]"), "_");
	if (sanitized.isEmpty()) {
		sanitized = "Game";
	}
	return sanitized;
}

#if defined(Q_OS_UNIX) && !defined(Q_OS_MAC)
QString escapeDesktopExecArg(const QString& arg) {
	QString escaped = arg;
	escaped.replace("\\", "\\\\");
	escaped.replace("\"", "\\\"");
	escaped.replace("$", "\\$");
	escaped.replace("`", "\\`");
	escaped.replace("%", "%%");
	return escaped;
}
#endif

#ifdef Q_OS_WIN
bool writePngAsIco(const QImage& sourceImage, const QString& icoPath) {
	QImage img = sourceImage;
	if (img.isNull()) {
		return false;
	}
	if (img.width() > 256 || img.height() > 256) {
		img = img.scaled(256, 256, Qt::KeepAspectRatio, Qt::SmoothTransformation);
	}
	QByteArray pngData;
	QBuffer buffer(&pngData);
	buffer.open(QIODevice::WriteOnly);
	if (!img.save(&buffer, "PNG")) {
		return false;
	}

	QFile file(icoPath);
	if (!file.open(QIODevice::WriteOnly)) {
		return false;
	}

	uint8_t header[22] = {0};
	// ICONDIR: reserved=0, type=1 (icon), count=1
	header[2] = 1;
	header[4] = 1;
	// ICONDIRENTRY
	header[6] = static_cast<uint8_t>(img.width() >= 256 ? 0 : img.width());
	header[7] = static_cast<uint8_t>(img.height() >= 256 ? 0 : img.height());
	header[10] = 1; // color planes
	header[12] = 32; // bits per pixel
	uint32_t dataSize = static_cast<uint32_t>(pngData.size());
	uint32_t offset = 22;
	header[14] = static_cast<uint8_t>(dataSize & 0xFF);
	header[15] = static_cast<uint8_t>((dataSize >> 8) & 0xFF);
	header[16] = static_cast<uint8_t>((dataSize >> 16) & 0xFF);
	header[17] = static_cast<uint8_t>((dataSize >> 24) & 0xFF);
	header[18] = static_cast<uint8_t>(offset & 0xFF);
	header[19] = static_cast<uint8_t>((offset >> 8) & 0xFF);
	header[20] = static_cast<uint8_t>((offset >> 16) & 0xFF);
	header[21] = static_cast<uint8_t>((offset >> 24) & 0xFF);

	file.write(reinterpret_cast<const char*>(header), sizeof(header));
	file.write(pngData);
	return true;
}
#endif

}

DesktopLinkDialog::DesktopLinkDialog(const QString& initialRomPath, const QString& initialName, QWidget* parent)
	: QDialog(parent)
{
	m_ui.setupUi(this);

	QString desktopDir = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
	if (desktopDir.isEmpty()) {
		desktopDir = QDir::homePath();
	}
	m_ui.destDir->setText(QDir::toNativeSeparators(desktopDir));

	if (!initialRomPath.isEmpty()) {
		m_ui.romPath->setText(QDir::toNativeSeparators(initialRomPath));
		QString autoName = initialName.isEmpty() ? QFileInfo(initialRomPath).completeBaseName() : initialName;
		m_ui.linkName->setText(autoName);
		m_lastAutoName = autoName;
	}

	connect(m_ui.browseRom, &QAbstractButton::clicked, this, &DesktopLinkDialog::browseRom);
	connect(m_ui.browseIcon, &QAbstractButton::clicked, this, &DesktopLinkDialog::browseIcon);
	connect(m_ui.browseDest, &QAbstractButton::clicked, this, &DesktopLinkDialog::browseDestination);

	connect(m_ui.romPath, &QLineEdit::textChanged, this, &DesktopLinkDialog::validate);
	connect(m_ui.linkName, &QLineEdit::textChanged, this, &DesktopLinkDialog::validate);
	connect(m_ui.destDir, &QLineEdit::textChanged, this, &DesktopLinkDialog::validate);
	connect(m_ui.iconPath, &QLineEdit::textChanged, this, &DesktopLinkDialog::updatePreview);

	connect(m_ui.buttonBox, &QDialogButtonBox::accepted, this, &DesktopLinkDialog::createLink);

	if (QPushButton* okButton = m_ui.buttonBox->button(QDialogButtonBox::Ok)) {
		okButton->setText(tr("Create Link"));
	}

	updatePreview();
	validate();
}

void DesktopLinkDialog::browseRom() {
	QString filename = GBAApp::app()->getOpenFileName(this, tr("Select Game ROM"), romFilters(true));
	if (filename.isEmpty()) {
		return;
	}
	m_ui.romPath->setText(QDir::toNativeSeparators(filename));
	QString currentName = m_ui.linkName->text().trimmed();
	if (currentName.isEmpty() || currentName == m_lastAutoName) {
		m_lastAutoName = QFileInfo(filename).completeBaseName();
		m_ui.linkName->setText(m_lastAutoName);
	}
}

void DesktopLinkDialog::browseIcon() {
	QString filter = tr("Images (*.png *.jpg *.jpeg *.bmp *.ico *.svg *.gif);;All files (*)");
	QString filename = GBAApp::app()->getOpenFileName(this, tr("Select Icon Image"), filter);
	if (!filename.isEmpty()) {
		m_ui.iconPath->setText(QDir::toNativeSeparators(filename));
	}
}

void DesktopLinkDialog::browseDestination() {
	QString dir = GBAApp::app()->getOpenDirectoryName(this, tr("Select Destination Folder"), m_ui.destDir->text());
	if (!dir.isEmpty()) {
		m_ui.destDir->setText(QDir::toNativeSeparators(dir));
	}
}

void DesktopLinkDialog::updatePreview() {
	QString path = QDir::fromNativeSeparators(m_ui.iconPath->text().trimmed());
	QPixmap pixmap;
	if (!path.isEmpty() && QFileInfo::exists(path)) {
		pixmap.load(path);
	}
	if (pixmap.isNull()) {
		pixmap.load(":/res/mgba-128.png");
	}
	if (!pixmap.isNull()) {
		m_ui.iconPreview->setPixmap(pixmap.scaled(60, 60, Qt::KeepAspectRatio, Qt::SmoothTransformation));
	} else {
		m_ui.iconPreview->clear();
	}
}

void DesktopLinkDialog::validate() {
	bool valid = !m_ui.romPath->text().trimmed().isEmpty()
		&& !m_ui.linkName->text().trimmed().isEmpty()
		&& !m_ui.destDir->text().trimmed().isEmpty();
	if (QPushButton* okButton = m_ui.buttonBox->button(QDialogButtonBox::Ok)) {
		okButton->setEnabled(valid);
	}
}

void DesktopLinkDialog::createLink() {
	QString romPath = QDir::fromNativeSeparators(m_ui.romPath->text().trimmed());
	QFileInfo romInfo(romPath);
	if (!romInfo.exists()) {
		QMessageBox::warning(this, tr("Invalid ROM"), tr("The selected ROM file does not exist."));
		return;
	}
	romPath = romInfo.absoluteFilePath();

	QString name = m_ui.linkName->text().trimmed();
	QString safeName = sanitizeFileName(name);
	QString destDirPath = QDir::fromNativeSeparators(m_ui.destDir->text().trimmed());
	QDir destDir(destDirPath);
	if (!destDir.exists() && !destDir.mkpath(".")) {
		QMessageBox::warning(this, tr("Invalid Destination"), tr("Could not access or create the destination folder."));
		return;
	}

	QString iconSrcPath = QDir::fromNativeSeparators(m_ui.iconPath->text().trimmed());
	if (!iconSrcPath.isEmpty() && !QFileInfo::exists(iconSrcPath)) {
		QMessageBox::warning(this, tr("Invalid Icon"), tr("The selected icon image file does not exist."));
		return;
	}

	QString iconStoreDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/desktop-icons";
	QDir().mkpath(iconStoreDir);
	QString hash = QString::fromLatin1(QCryptographicHash::hash((name + "|" + romPath).toUtf8(), QCryptographicHash::Sha1).toHex().left(10));
	QString exePath = QCoreApplication::applicationFilePath();

#if defined(Q_OS_WIN)
	QString icoFilePath = iconStoreDir + "/" + safeName + "-" + hash + ".ico";
	QImage iconImage;
	if (!iconSrcPath.isEmpty()) {
		iconImage.load(iconSrcPath);
	}
	if (iconImage.isNull()) {
		iconImage.load(":/res/mgba-256.png");
	}
	bool hasIcon = writePngAsIco(iconImage, icoFilePath);

	QString lnkPath = QDir::toNativeSeparators(destDir.filePath(safeName + ".lnk"));
	HRESULT hrInit = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
	IShellLinkW* shellLink = nullptr;
	HRESULT hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW, reinterpret_cast<void**>(&shellLink));
	bool ok = false;
	if (SUCCEEDED(hr) && shellLink) {
		QString nativeExe = QDir::toNativeSeparators(exePath);
		QString args = "\"" + QDir::toNativeSeparators(romPath) + "\"";
		QString workDir = QDir::toNativeSeparators(QFileInfo(exePath).absolutePath());
		shellLink->SetPath(reinterpret_cast<LPCWSTR>(nativeExe.utf16()));
		shellLink->SetArguments(reinterpret_cast<LPCWSTR>(args.utf16()));
		shellLink->SetWorkingDirectory(reinterpret_cast<LPCWSTR>(workDir.utf16()));
		shellLink->SetDescription(reinterpret_cast<LPCWSTR>(name.utf16()));
		if (hasIcon) {
			QString nativeIco = QDir::toNativeSeparators(icoFilePath);
			shellLink->SetIconLocation(reinterpret_cast<LPCWSTR>(nativeIco.utf16()), 0);
		}
		IPersistFile* persistFile = nullptr;
		if (SUCCEEDED(shellLink->QueryInterface(IID_IPersistFile, reinterpret_cast<void**>(&persistFile))) && persistFile) {
			ok = SUCCEEDED(persistFile->Save(reinterpret_cast<LPCWSTR>(lnkPath.utf16()), TRUE));
			persistFile->Release();
		}
		shellLink->Release();
	}
	if (SUCCEEDED(hrInit)) {
		CoUninitialize();
	}
	if (!ok) {
		QMessageBox::warning(this, tr("Error"), tr("Failed to create Windows shortcut at:\n%1").arg(lnkPath));
		return;
	}
	QMessageBox::information(this, tr("Desktop Link Created"), tr("Created desktop link:\n%1").arg(lnkPath));
	accept();
#elif defined(Q_OS_MAC)
	QString appPath = destDir.filePath(safeName + ".app");
	QDir appDir(appPath);
	QString contentsDir = appDir.filePath("Contents");
	QString macosDir = appDir.filePath("Contents/MacOS");
	QString resDir = appDir.filePath("Contents/Resources");
	if (!QDir().mkpath(macosDir) || !QDir().mkpath(resDir)) {
		QMessageBox::warning(this, tr("Error"), tr("Failed to create app bundle at:\n%1").arg(appPath));
		return;
	}

	QString execTarget = macosDir + "/" + safeName;
	QFile execFile(execTarget);
	if (!execFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		QMessageBox::warning(this, tr("Error"), tr("Failed to write launcher script."));
		return;
	}
	QString escapedExe = exePath;
	escapedExe.replace("\"", "\\\"");
	QString escapedRom = romPath;
	escapedRom.replace("\"", "\\\"");
	QString script = QString("#!/bin/sh\nexec \"%1\" \"%2\"\n").arg(escapedExe, escapedRom);
	execFile.write(script.toUtf8());
	execFile.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner
		| QFileDevice::ReadGroup | QFileDevice::ExeGroup
		| QFileDevice::ReadOther | QFileDevice::ExeOther);
	execFile.close();

	QString iconFileName = "icon.icns";
	QString icnsPath = resDir + "/" + iconFileName;
	bool iconWritten = false;
	if (!iconSrcPath.isEmpty()) {
		QString tmpPng = iconStoreDir + "/" + safeName + "-" + hash + ".png";
		QImage img(iconSrcPath);
		if (!img.isNull() && img.save(tmpPng, "PNG")) {
			iconWritten = (QProcess::execute("sips", {"-s", "format", "icns", tmpPng, "--out", icnsPath}) == 0);
		}
	}
	if (!iconWritten) {
		QFile::copy(":/res/mgba.icns", icnsPath);
	}

	QString plist = QString(
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
		"<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
		"<plist version=\"1.0\">\n"
		"<dict>\n"
		"\t<key>CFBundleExecutable</key>\n\t<string>%1</string>\n"
		"\t<key>CFBundleName</key>\n\t<string>%2</string>\n"
		"\t<key>CFBundleIconFile</key>\n\t<string>%3</string>\n"
		"\t<key>CFBundlePackageType</key>\n\t<string>APPL</string>\n"
		"</dict>\n"
		"</plist>\n").arg(safeName.toHtmlEscaped(), name.toHtmlEscaped(), iconFileName);
	QFile plistFile(contentsDir + "/Info.plist");
	if (plistFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		plistFile.write(plist.toUtf8());
		plistFile.close();
	}

	QMessageBox::information(this, tr("Desktop Link Created"), tr("Created desktop link:\n%1").arg(appPath));
	accept();
#else
	QString savedIconPath;
	if (!iconSrcPath.isEmpty()) {
		QFileInfo iconInfo(iconSrcPath);
		if (iconInfo.suffix().compare("svg", Qt::CaseInsensitive) == 0) {
			savedIconPath = iconStoreDir + "/" + safeName + "-" + hash + ".svg";
			QFile::remove(savedIconPath);
			if (!QFile::copy(iconSrcPath, savedIconPath)) {
				savedIconPath = iconSrcPath;
			}
		} else {
			QImage img(iconSrcPath);
			if (img.isNull()) {
				QMessageBox::warning(this, tr("Invalid Icon"), tr("Could not load the selected icon image."));
				return;
			}
			savedIconPath = iconStoreDir + "/" + safeName + "-" + hash + ".png";
			if (!img.save(savedIconPath, "PNG")) {
				savedIconPath = iconSrcPath;
			}
		}
	} else {
		savedIconPath = iconStoreDir + "/mgba-default.png";
		if (!QFileInfo::exists(savedIconPath)) {
			QImage defaultImg(":/res/mgba-256.png");
			if (!defaultImg.isNull()) {
				defaultImg.save(savedIconPath, "PNG");
			} else {
				savedIconPath = "io.mgba.mGBA";
			}
		}
	}

	QString desktopFilePath = destDir.filePath(safeName + ".desktop");
	QFile desktopFile(desktopFilePath);
	if (!desktopFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
		QMessageBox::warning(this, tr("Error"), tr("Failed to create desktop file at:\n%1").arg(desktopFilePath));
		return;
	}

	QString desktopName = name;
	desktopName.replace('\n', ' ').replace('\r', ' ');
	QString execLine = QString("\"%1\" \"%2\"").arg(escapeDesktopExecArg(exePath), escapeDesktopExecArg(romPath));

	QString content = QString(
		"[Desktop Entry]\n"
		"Version=1.0\n"
		"Type=Application\n"
		"Name=%1\n"
		"Exec=%2\n"
		"Icon=%3\n"
		"Terminal=false\n"
		"Categories=Game;Emulator;\n"
		"StartupWMClass=mGBA\n").arg(desktopName, execLine, savedIconPath);

	desktopFile.write(content.toUtf8());
	desktopFile.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner
		| QFileDevice::ReadGroup | QFileDevice::ExeGroup
		| QFileDevice::ReadOther | QFileDevice::ExeOther);
	desktopFile.close();

	QMessageBox::information(this, tr("Desktop Link Created"), tr("Created desktop link:\n%1").arg(desktopFilePath));
	accept();
#endif
}
