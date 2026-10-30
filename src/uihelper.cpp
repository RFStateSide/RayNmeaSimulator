/*
 * RayNmeaSimulator - NMEA2000/NMEA0183 Marine Simulator
 *
 * Copyright (C) 2024-2026 RF StateSide LLC
 *
 * This file is part of RayNmeaSimulator.
 *
 * RayNmeaSimulator is free software: you can redistribute it
 * and/or modify it under the terms of the GNU Affero General
 * Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any
 * later version.
 *
 * RayNmeaSimulator is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY; without even the
 * implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE. See the GNU Affero General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Affero General
 * Public License along with RayNmeaSimulator.
 * If not, see <https://www.gnu.org/licenses/>.
 */

#include <QPointer>
#include "uihelper.h"
#include "apppaths.h"
#include <QFileInfo>

UiHelper::UiHelper() {}

int UiHelper::ShowSpinBoxDialog(const QString &title, const QString &labelText, int minValue, int maxValue, int defaultValue)
{
    // Create a dialog
    QDialog dialog;
    dialog.setWindowTitle(title);

    // Set up the layout
    QVBoxLayout layout(&dialog);

    // Add a label to the dialog
    QLabel label(labelText);
    layout.addWidget(&label);

    // Add a spin box to the dialog
    QSpinBox spinBox;
    spinBox.setRange(minValue, maxValue);
    spinBox.setValue(defaultValue);
    layout.addWidget(&spinBox);

    // Add OK and Cancel buttons
    QHBoxLayout buttonLayout;
    QPushButton okButton("OK");
    QPushButton cancelButton("Cancel");

    // Add buttons to the button layout
    buttonLayout.addWidget(&okButton);
    buttonLayout.addWidget(&cancelButton);

    // Add the button layout to the main layout
    layout.addLayout(&buttonLayout);

    // Connect buttons
    QObject::connect(&okButton, &QPushButton::clicked, [&dialog]() { dialog.accept(); });
    QObject::connect(&cancelButton, &QPushButton::clicked, [&dialog]() { dialog.reject(); });

    // Show the dialog
    if (dialog.exec() == QDialog::Accepted) {
        // Return the selected number
        return spinBox.value();
    } else {
        // Return -1 if the dialog was canceled
        return -1;
    }
}

int UiHelper::ShowTemperatureSourcePicker(const QString &title, const QString &labelText, int defaultIndex)
{
    // Create a dialog
    QDialog dialog;
    dialog.setWindowTitle(title);

    // Set up the layout
    QVBoxLayout layout(&dialog);

    // Add a label to the dialog
    QLabel label(labelText);
    layout.addWidget(&label);

    // Add a combo box with temperature sources
    QComboBox comboBox;
    comboBox.addItem("Sea Temperature", N2kts_SeaTemperature);
    comboBox.addItem("Outside Temperature", N2kts_OutsideTemperature);
    comboBox.addItem("Inside Temperature", N2kts_InsideTemperature);
    comboBox.addItem("Engine Room Temperature", N2kts_EngineRoomTemperature);
    comboBox.addItem("Main Cabin Temperature", N2kts_MainCabinTemperature);
    comboBox.addItem("Live Well Temperature", N2kts_LiveWellTemperature);
    comboBox.addItem("Bait Well Temperature", N2kts_BaitWellTemperature);
    comboBox.addItem("Refrigeration Temperature", N2kts_RefridgerationTemperature);
    comboBox.addItem("Heating System Temperature", N2kts_HeatingSystemTemperature);
    comboBox.addItem("Dew Point Temperature", N2kts_DewPointTemperature);
    comboBox.addItem("Apparent Wind Chill Temperature", N2kts_ApparentWindChillTemperature);
    comboBox.addItem("Theoretical Wind Chill Temperature", N2kts_TheoreticalWindChillTemperature);
    comboBox.addItem("Heat Index Temperature", N2kts_HeatIndexTemperature);
    comboBox.addItem("Freezer Temperature", N2kts_FreezerTemperature);
    comboBox.addItem("Exhaust Gas Temperature", N2kts_ExhaustGasTemperature);
    comboBox.addItem("Shaft Seal Temperature", N2kts_ShaftSealTemperature);

    // Set the default index
    comboBox.setCurrentIndex(defaultIndex);
    layout.addWidget(&comboBox);

    // Add OK and Cancel buttons
    QHBoxLayout buttonLayout;
    QPushButton okButton("OK");
    QPushButton cancelButton("Cancel");

    // Add buttons to the button layout
    buttonLayout.addWidget(&okButton);
    buttonLayout.addWidget(&cancelButton);

    // Add the button layout to the main layout
    layout.addLayout(&buttonLayout);

    // Connect buttons
    QObject::connect(&okButton, &QPushButton::clicked, [&dialog]() { dialog.accept(); });
    QObject::connect(&cancelButton, &QPushButton::clicked, [&dialog]() { dialog.reject(); });

    // Show the dialog
    if (dialog.exec() == QDialog::Accepted) {
        // Return the selected temperature source
        return comboBox.currentData().toInt();
    } else {
        // Return -1 if the dialog was canceled
        return -1;
    }
}

int UiHelper::ShowFluidLevelSourcePicker(const QString &title, const QString &labelText, int defaultIndex)
{
    // Create a dialog
    QDialog dialog;
    dialog.setWindowTitle(title);

    // Set up the layout
    QVBoxLayout layout(&dialog);

    // Add a label to the dialog
    QLabel label(labelText);
    layout.addWidget(&label);

    // Add a combo box with fluid level sources
    QComboBox comboBox;
    comboBox.addItem("Fuel", N2kft_Fuel);
    comboBox.addItem("Water", N2kft_Water);
    comboBox.addItem("Gray Water", N2kft_GrayWater);
    comboBox.addItem("Live Well", N2kft_LiveWell);
    comboBox.addItem("Oil", N2kft_Oil);
    comboBox.addItem("Black Water", N2kft_BlackWater);
    comboBox.addItem("Gasoline Fuel", N2kft_FuelGasoline);

    // Set the default index
    comboBox.setCurrentIndex(defaultIndex);
    layout.addWidget(&comboBox);

    // Add OK and Cancel buttons
    QHBoxLayout buttonLayout;
    QPushButton okButton("OK");
    QPushButton cancelButton("Cancel");

    // Add buttons to the button layout
    buttonLayout.addWidget(&okButton);
    buttonLayout.addWidget(&cancelButton);

    // Add the button layout to the main layout
    layout.addLayout(&buttonLayout);

    // Connect buttons
    QObject::connect(&okButton, &QPushButton::clicked, [&dialog]() { dialog.accept(); });
    QObject::connect(&cancelButton, &QPushButton::clicked, [&dialog]() { dialog.reject(); });

    // Show the dialog
    if (dialog.exec() == QDialog::Accepted) {
        // Return the selected fluid level source
        return comboBox.currentData().toInt();
    } else {
        // Return -1 if the dialog was canceled
        return -1;
    }
}

QString UiHelper::GetLayoutNameInput(QWidget *parent, const QString &title, const QString &message, const QString &defaultFileName)
{
    // Regular expression for valid file name characters on both Windows and Unix
    QRegularExpression validFileNameRegex(R"(^[^<>:\"/\\|?*\n]+$)");

    // Validator using the regular expression
    QRegularExpressionValidator validator(validFileNameRegex, nullptr);

    bool ok;
    QString fileName;

    // Prompt user for file name input
    while (true) {
        fileName = QInputDialog::getText(parent, title, message, QLineEdit::Normal, defaultFileName, &ok);

        // If user cancels the input dialog
        if (!ok) {
            return QString(); // Return an empty string to indicate cancellation
        }

        // Validate the input file name
        int pos = 0;
        if (validator.validate(fileName, pos) == QValidator::Acceptable) {
            return fileName; // Valid file name, return it
        } else if (fileName.contains('.')) {
            // Show an error message if the name contains an extension
            QMessageBox::warning(parent, "Invalid Name", "Please enter a name without an extension (e.g., '.json').");
        } else {
            // Show an error message if the file name is invalid
            QMessageBox::warning(parent, "Invalid Name", "The name contains invalid characters. " "Please avoid using any of the following: <>:\"/\\|?*\n.");
        }
    }
}

QString UiHelper::ShowLayoutPicker(const QString &title, const QString &labelText)
{
    // Create a dialog for layout picker
    QDialog dialog;
    dialog.setWindowTitle(title);

    // Create combo box and label
    QComboBox *comboBox = new QComboBox(&dialog);
    comboBox->setEditable(false);

    const auto files = AppPaths::layoutFiles();
    for (const auto &file : files) {
        int matches = 0;
        for (const auto &other : files)
            if (other.fileName().compare(file.fileName(), Qt::CaseInsensitive) == 0) ++matches;
        const bool current = file.absolutePath() == AppPaths::layoutsDirectory();
        const QString name = file.completeBaseName();
        const QString display = matches > 1 ? name + (current ? " (current data)" : " (legacy)") : name;
        comboBox->addItem(display, file.absoluteFilePath());
        comboBox->setItemData(comboBox->count()-1, QDir::toNativeSeparators(file.absoluteFilePath()), Qt::ToolTipRole);
    }
    comboBox->setObjectName("layoutFileChoices");
    // Set up layout
    QVBoxLayout *layout = new QVBoxLayout;
    layout->addWidget(new QLabel(labelText + "\nData: " + QDir::toNativeSeparators(AppPaths::layoutsDirectory()), &dialog));
    layout->addWidget(comboBox);

    // Add standard dialog buttons
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttonBox);

    // Connect the dialog buttons
    QObject::connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    // Set dialog layout
    dialog.setLayout(layout);

    // Show dialog and return the selected file name if "Ok" is clicked
    if (dialog.exec() == QDialog::Accepted) {
        return comboBox->currentData().toString(); // Full path; duplicate basenames remain independently selectable.
    }

    return QString(); // Return empty string if "Cancel" is clicked
}

QString UiHelper::ShowGroupPicker(const QString &title, const QString &labelText, const QStringList &groupList, const QString &initialGroup)
{
    // Create a dialog for group selection
    QDialog dialog;
    dialog.setWindowTitle(title);

    // Create combo box and label
    QComboBox *comboBox = new QComboBox(&dialog);
    comboBox->addItems(groupList);
    const int initialIndex = comboBox->findText(initialGroup);
    if (initialIndex >= 0) comboBox->setCurrentIndex(initialIndex);

    // Set up layout
    QVBoxLayout *layout = new QVBoxLayout;
    layout->addWidget(new QLabel(labelText, &dialog));
    layout->addWidget(comboBox);

    // Add dialog buttons
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttonBox);

    // Connect dialog buttons to dialog slots
    QObject::connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    // Set dialog layout
    dialog.setLayout(layout);

    // Show dialog and return the selected group if "Ok" is clicked
    if (dialog.exec() == QDialog::Accepted) {
        return comboBox->currentText(); // Returns the selected group name
    }

    return QString(); // Return an empty string if "Cancel" is clicked
}

QString UiHelper::ShowStationPicker(const QString &title, const QString &labelText, const QList<Station> &stations, QWidget* owner)
{
    // Create a dialog for station picker
    QPointer<QDialog> dialog = new QDialog(owner);
    dialog->setObjectName("weatherStationPicker");
    dialog->setWindowTitle(title);

    // Create combo box and label
    QComboBox *comboBox = new QComboBox(dialog.data());
    comboBox->setEditable(false);

    // Populate combo box with station names, associating each name with its corresponding ID
    for (const Station &station : stations) {
        comboBox->addItem(station.name, station.id); // Store the station ID as user data
    }

    // Set up layout
    QVBoxLayout *layout = new QVBoxLayout;
    layout->addWidget(new QLabel(labelText, dialog.data()));
    layout->addWidget(comboBox);

    // Add standard dialog buttons
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dialog.data());
    layout->addWidget(buttonBox);

    // Connect the dialog buttons
    QObject::connect(buttonBox, &QDialogButtonBox::accepted, dialog.data(), &QDialog::accept);
    QObject::connect(buttonBox, &QDialogButtonBox::rejected, dialog.data(), &QDialog::reject);

    // Set dialog layout
    dialog->setLayout(layout);

    // Show dialog and return the selected station ID if "Ok" is clicked
    const int result = dialog->exec();
    if (!dialog) return QString(); // Owner/layout may have been destroyed in the modal loop.
    const QString id = result == QDialog::Accepted ? comboBox->currentData().toString() : QString();
    delete dialog.data();
    return id;
}
