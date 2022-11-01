/*
 * Copyright (C) 2022 Uniontech Software Technology Co., Ltd.
 *
 * Author:     lixiang<lixianga@uniontech.com>
 *
 * Maintainer: lixiang<lixianga@uniontech.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "propertyeventreceiver.h"
#include "utils/propertydialogutil.h"
#include "utils/propertydialogmanager.h"

#include <dfm-framework/dpf.h>

using namespace dfmplugin_propertydialog;

PropertyEventReceiver::PropertyEventReceiver(QObject *parent)
    : QObject(parent)
{
}

PropertyEventReceiver *PropertyEventReceiver::instance()
{
    static PropertyEventReceiver receiver;
    return &receiver;
}

void PropertyEventReceiver::bindEvents()
{
    dpfSlotChannel->connect(DPF_MACRO_TO_STR(DPPROPERTYDIALOG_NAMESPACE), "slot_PropertyDialog_Show",
                            this, &PropertyEventReceiver::handleShowPropertyDialog);

    dpfSlotChannel->connect(DPF_MACRO_TO_STR(DPPROPERTYDIALOG_NAMESPACE), "slot_ViewExtension_Register",
                            this, &PropertyEventReceiver::handleViewExtensionRegister);
    dpfSlotChannel->connect(DPF_MACRO_TO_STR(DPPROPERTYDIALOG_NAMESPACE), "slot_ViewExtension_Unregister",
                            this, &PropertyEventReceiver::handleViewExtensionUnregister);

    dpfSlotChannel->connect(DPF_MACRO_TO_STR(DPPROPERTYDIALOG_NAMESPACE), "slot_CustomView_Register",
                            this, &PropertyEventReceiver::handleCustomViewRegister);
    dpfSlotChannel->connect(DPF_MACRO_TO_STR(DPPROPERTYDIALOG_NAMESPACE), "slot_CustomView_UnRegister",
                            this, &PropertyEventReceiver::handleCustomViewUnregister);

    dpfSlotChannel->connect(DPF_MACRO_TO_STR(DPPROPERTYDIALOG_NAMESPACE), "slot_BasicViewExtension_Register",
                            this, &PropertyEventReceiver::handleBasicViewExtensionRegister);
    dpfSlotChannel->connect(DPF_MACRO_TO_STR(DPPROPERTYDIALOG_NAMESPACE), "slot_BasicViewExtension_Unregister",
                            this, &PropertyEventReceiver::handleBasicViewExtensionUnregister);

    dpfSlotChannel->connect(DPF_MACRO_TO_STR(DPPROPERTYDIALOG_NAMESPACE), "slot_BasicFiledFilter_Add",
                            this, &PropertyEventReceiver::handleBasicFiledFilterAdd);
    dpfSlotChannel->connect(DPF_MACRO_TO_STR(DPPROPERTYDIALOG_NAMESPACE), "slot_BasicFiledFilter_Remove",
                            this, &PropertyEventReceiver::handleBasicFiledFilterRemove);
}

void PropertyEventReceiver::handleShowPropertyDialog(const QList<QUrl> &urls, const QVariantHash &option)
{
    PropertyDialogUtil *fileDialogManager = PropertyDialogUtil::instance();

    QVariantHash showViewOption;
    if (!option.isEmpty()) {   // Need adjust the registered initOption.
        const QString &name = option.value(kOption_Key_Name).toString();   // The `name` was registed when registered view creator.
        showViewOption = PropertyDialogManager::instance().getCreatorOptionByName(name);   // get the registered initial option
        const QStringList &initialOptKeys = showViewOption.keys();
        const QStringList &inputOptKeys = option.keys();
        for (const QString &key : inputOptKeys) {
            if (initialOptKeys.contains(key)) {
                showViewOption.insert(key, option.value(key));   // update the default option by the input option
            }
        }
    }
    fileDialogManager->showPropertyDialog(urls, showViewOption);   // Show the property dialog with updated option
}

bool PropertyEventReceiver::handleViewExtensionRegister(CustomViewExtensionView view, const QString &name, int index)
{
    return PropertyDialogManager::instance().registerExtensionView(view, name, index);
}

void PropertyEventReceiver::handleViewExtensionUnregister(int index)
{
    PropertyDialogManager::instance().unregisterExtensionView(index);
}

bool PropertyEventReceiver::handleCustomViewRegister(CustomViewExtensionView view, const QString &scheme)
{
    return PropertyDialogManager::instance().registerCustomView(view, scheme);
}

void PropertyEventReceiver::handleCustomViewUnregister(const QString &scheme)
{
    PropertyDialogManager::instance().unregisterCustomView(scheme);
}

bool PropertyEventReceiver::handleBasicViewExtensionRegister(BasicViewFieldFunc func, const QString &scheme)
{
    return PropertyDialogManager::instance().registerBasicViewExtension(func, scheme);
}

void PropertyEventReceiver::handleBasicViewExtensionUnregister(const QString &scheme)
{
    PropertyDialogManager::instance().unregisterBasicViewExtension(scheme);
}

bool PropertyEventReceiver::handleBasicFiledFilterAdd(const QString &scheme, const QStringList &enums)
{
    QMetaEnum &&metaState { QMetaEnum::fromType<PropertyFilterType>() };
    QString &&join { enums.join("|") };
    bool ok { false };

    auto &&enumValues { static_cast<PropertyFilterType>(metaState.keysToValue(join.toUtf8().constData(), &ok)) };
    if (ok)
        ok = PropertyDialogManager::instance().addBasicFiledFiltes(scheme, enumValues);

    return ok;
}

void PropertyEventReceiver::handleBasicFiledFilterRemove(const QString &scheme)
{
    PropertyDialogManager::instance().removeBasicFiledFilters(scheme);
}
