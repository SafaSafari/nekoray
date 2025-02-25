#include "dialog_edit_profile.h"
#include "ui_dialog_edit_profile.h"

#include "ui/edit/edit_socks_http.h"
#include "ui/edit/edit_shadowsocks.h"
#include "ui/edit/edit_chain.h"
#include "ui/edit/edit_vmess.h"
#include "ui/edit/edit_trojan_vless.h"
#include "ui/edit/edit_naive.h"
#include "ui/edit/edit_custom.h"

#include "fmt/includes.h"

#include "qv2ray/v2/ui/widgets/editors/w_JsonEditor.hpp"
#include "main/GuiUtils.hpp"

#include <QInputDialog>

#define ADJUST_SIZE runOnUiThread([=] { adjustSize(); adjustPosition(mainwindow); }, this);
#define LOAD_TYPE(a) ui->type->addItem(NekoRay::ProfileManager::NewProxyEntity(a)->bean->DisplayType(), a);

DialogEditProfile::DialogEditProfile(const QString &_type, int profileOrGroupId, QWidget *parent)
        : QDialog(parent),
          ui(new Ui::DialogEditProfile) {
    // setup UI
    ui->setupUi(this);
    ui->dialog_layout->setAlignment(ui->left, Qt::AlignTop);

    // network changed
    network_title_base = ui->network_box->title();
    connect(ui->network, &QComboBox::currentTextChanged, this, [=](const QString &txt) {
        ui->network_box->setTitle(network_title_base.arg(txt));
        if (txt == "tcp" || txt == "quic") {
            ui->header_type->setVisible(true);
            ui->header_type_l->setVisible(true);
            ui->path->setVisible(true);
            ui->path_l->setVisible(true);
            ui->host->setVisible(true);
            ui->host_l->setVisible(true);
        } else if (txt == "grpc") {
            ui->header_type->setVisible(false);
            ui->header_type_l->setVisible(false);
            ui->path->setVisible(true);
            ui->path_l->setVisible(true);
            ui->host->setVisible(false);
            ui->host_l->setVisible(false);
        } else if (txt == "ws" || txt == "h2") {
            ui->header_type->setVisible(false);
            ui->header_type_l->setVisible(false);
            ui->path->setVisible(true);
            ui->path_l->setVisible(true);
            ui->host->setVisible(true);
            ui->host_l->setVisible(true);
        } else {
            ui->header_type->setVisible(false);
            ui->header_type_l->setVisible(false);
            ui->path->setVisible(false);
            ui->path_l->setVisible(false);
            ui->host->setVisible(false);
            ui->host_l->setVisible(false);
        }
        if (txt == "ws") {
            ui->ws_early_data_length->setVisible(true);
            ui->ws_early_data_length_l->setVisible(true);
            ui->ws_early_data_name->setVisible(true);
            ui->ws_early_data_name_l->setVisible(true);
        } else {
            ui->ws_early_data_length->setVisible(false);
            ui->ws_early_data_length_l->setVisible(false);
            ui->ws_early_data_name->setVisible(false);
            ui->ws_early_data_name_l->setVisible(false);
        }
        ADJUST_SIZE
    });
    ui->network->removeItem(0);

    // security changed
    connect(ui->security, &QComboBox::currentTextChanged, this, [=](const QString &txt) {
        if (txt == "tls") {
            ui->security_box->setVisible(true);
        } else {
            ui->security_box->setVisible(false);
        }
        ADJUST_SIZE
    });

    // 确定模式和 ent
    newEnt = _type != "";
    if (newEnt) {
        this->groupId = profileOrGroupId;
        this->type = _type;

        // load type to combo box
        LOAD_TYPE("socks")
        LOAD_TYPE("http")
        LOAD_TYPE("shadowsocks");
        LOAD_TYPE("trojan");
        LOAD_TYPE("vmess");
        LOAD_TYPE("vless");
        LOAD_TYPE("naive");
        ui->type->addItem("Hysteria", "hysteria");
        ui->type->addItem(tr("Custom (%1)").arg(software_core_name), "internal");
        ui->type->addItem(tr("Custom (Extra Core)"), "custom");
        LOAD_TYPE("chain");

        // type changed
        connect(ui->type, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [=](int index) {
            typeSelected(ui->type->itemData(index).toString());
        });
    } else {
        this->ent = NekoRay::profileManager->GetProfile(profileOrGroupId);
        if (this->ent == nullptr) return;
        this->type = ent->type;
        ui->type->setVisible(false);
        ui->type_l->setVisible(false);
    }

    typeSelected(this->type);
}

DialogEditProfile::~DialogEditProfile() {
    delete ui;
}

void DialogEditProfile::typeSelected(const QString &newType) {
    QString customType;
    type = newType;
    bool validType = true;

    if (type == "socks" || type == "http") {
        auto _innerWidget = new EditSocksHttp(this);
        innerWidget = _innerWidget;
        innerEditor = _innerWidget;
    } else if (type == "shadowsocks") {
        auto _innerWidget = new EditShadowSocks(this);
        innerWidget = _innerWidget;
        innerEditor = _innerWidget;
    } else if (type == "chain") {
        auto _innerWidget = new EditChain(this);
        innerWidget = _innerWidget;
        innerEditor = _innerWidget;
    } else if (type == "vmess") {
        auto _innerWidget = new EditVMess(this);
        innerWidget = _innerWidget;
        innerEditor = _innerWidget;
    } else if (type == "trojan" || type == "vless") {
        auto _innerWidget = new EditTrojanVLESS(this);
        innerWidget = _innerWidget;
        innerEditor = _innerWidget;
    } else if (type == "naive") {
        auto _innerWidget = new EditNaive(this);
        innerWidget = _innerWidget;
        innerEditor = _innerWidget;
    } else if (type == "custom" || type == "internal" || type == "hysteria") {
        auto _innerWidget = new EditCustom(this);
        innerWidget = _innerWidget;
        innerEditor = _innerWidget;
        customType = newEnt ? type : ent->CustomBean()->core;
        if (customType != "custom") _innerWidget->preset_core = customType;
        type = "custom";
    } else {
        validType = false;
    }

    if (!validType) {
        MessageBoxWarning(newType, "Wrong type");
        return;
    }

    if (newEnt) {
        this->ent = NekoRay::ProfileManager::NewProxyEntity(type);
        this->ent->gid = groupId;
    }

    // hide some widget
    auto showAddressPort = type != "chain" && customType != "internal";
    ui->address->setVisible(showAddressPort);
    ui->address_l->setVisible(showAddressPort);
    ui->port->setVisible(showAddressPort);
    ui->port_l->setVisible(showAddressPort);

    // 右边 Outbound: settings
    auto stream = GetStreamSettings(ent->bean.data());
    if (stream != nullptr) {
        ui->right_all_w->setVisible(true);
        ui->network->setCurrentText(stream->network);
        ui->security->setCurrentText(stream->security);
        ui->packet_encoding->setCurrentText(stream->packet_encoding);
        ui->path->setText(stream->path);
        ui->host->setText(stream->host);
        ui->sni->setText(stream->sni);
        ui->alpn->setText(stream->alpn);
        ui->utls->setCurrentText(stream->utls);
        ui->insecure->setChecked(stream->allow_insecure);
        ui->header_type->setCurrentText(stream->header_type);
        ui->ws_early_data_name->setText(stream->ws_early_data_name);
        ui->ws_early_data_length->setText(Int2String(stream->ws_early_data_length));
        CACHE.certificate = stream->certificate;
    } else {
        ui->right_all_w->setVisible(false);
    }

    // left: custom
    auto custom_item = ent->bean->_get("custom");
    if (custom_item != nullptr) {
        ui->custom_box->setVisible(true);
        CACHE.custom = *((QString *) custom_item->ptr);
    } else {
        ui->custom_box->setVisible(false);
    }

    // 左边 bean
    auto old = ui->bean->layout()->itemAt(0)->widget();
    ui->bean->layout()->removeWidget(old);
    innerWidget->layout()->setContentsMargins(0, 0, 0, 0);
    ui->bean->layout()->addWidget(innerWidget);
    ui->bean->setTitle(ent->bean->DisplayType());
    delete old;

    // 左边 bean inner editor
    innerEditor->get_edit_dialog = [&]() {
        return (QWidget *) this;
    };
    innerEditor->editor_cache_updated = [=] {
        editor_cache_updated_impl();
    };
    innerEditor->onStart(ent);

    // 左边 common
    ui->name->setText(ent->bean->name);
    ui->address->setText(ent->bean->serverAddress);
    ui->port->setText(Int2String(ent->bean->serverPort));
    ui->port->setValidator(QRegExpValidator_Number, this));

    // 星号
    for (auto label: findChildren<QLabel *>()) {
        auto text = label->text();
        if (!label->toolTip().isEmpty() && !text.endsWith("*")) {
            label->setText(text + "*");
        }
    }

    if (IS_NEKO_BOX) {
        ui->header_type->hide();
        ui->header_type_l->hide();
        //
        if (type == "vmess" || type == "vless") {
            ui->packet_encoding->setVisible(true);
            ui->packet_encoding_l->setVisible(true);
        } else {
            ui->packet_encoding->setVisible(false);
            ui->packet_encoding_l->setVisible(false);
        }
        if (type == "vmess" || type == "vless" || type == "trojan") {
            ui->network_l->setVisible(true);
            ui->network->setVisible(true);
            ui->network_box->setVisible(true);
        } else {
            ui->network_l->setVisible(false);
            ui->network->setVisible(false);
            ui->network_box->setVisible(false);
        }
        if (type == "vmess" || type == "vless" || type == "trojan" || type == "http") {
            ui->security->setVisible(true);
            ui->security_l->setVisible(true);
        } else {
            ui->security->setVisible(false);
            ui->security_l->setVisible(false);
        }
        //
        int streamBoxVisible = 0;
        for (auto label: ui->stream_box->findChildren<QLabel *>()) {
            if (!label->isHidden()) streamBoxVisible++;
        }
        ui->stream_box->setVisible(streamBoxVisible);
    }

    auto rightNoBox = (ui->stream_box->isHidden() && ui->security_box->isHidden() && ui->network_box->isHidden());
    if (rightNoBox && !ui->right_all_w->isHidden()) {
        ui->right_all_w->setVisible(false);
    }

    editor_cache_updated_impl();
    ADJUST_SIZE

    // 第一次显示
    if (isHidden()) {
        show();
    }
}

void DialogEditProfile::accept() {
    // 左边
    ent->bean->name = ui->name->text();
    ent->bean->serverAddress = ui->address->text();
    ent->bean->serverPort = ui->port->text().toInt();

    // bean
    if (!innerEditor->onEnd()) {
        return;
    }

    // 右边
    auto stream = GetStreamSettings(ent->bean.data());
    if (stream != nullptr) {
        stream->network = ui->network->currentText();
        stream->security = ui->security->currentText();
        stream->packet_encoding = ui->packet_encoding->currentText();
        stream->path = ui->path->text();
        stream->host = ui->host->text();
        stream->sni = ui->sni->text();
        stream->alpn = ui->alpn->text();
        stream->utls = ui->utls->currentText();
        stream->allow_insecure = ui->insecure->isChecked();
        stream->header_type = ui->header_type->currentText();
        stream->ws_early_data_name = ui->ws_early_data_name->text();
        stream->ws_early_data_length = ui->ws_early_data_length->text().toInt();
        stream->certificate = CACHE.certificate;
    }
    auto custom_item = ent->bean->_get("custom");
    if (custom_item != nullptr) {
        *((QString *) custom_item->ptr) = CACHE.custom;
    }

    if (newEnt) {
        auto ok = NekoRay::profileManager->AddProfile(ent);
        if (!ok) {
            MessageBoxWarning("???", "id exists");
        }
    } else {
        ent->Save();
    }

    MW_dialog_message(Dialog_DialogEditProfile, "accept");
    QDialog::accept();
}

// cached editor (dialog)

void DialogEditProfile::editor_cache_updated_impl() {
    if (CACHE.certificate.isEmpty()) {
        ui->certificate_edit->setText(tr("Not set"));
    } else {
        ui->certificate_edit->setText(tr("Already set"));
    }
    if (CACHE.custom.isEmpty()) {
        ui->custom_edit->setText(tr("Not set"));
    } else {
        ui->custom_edit->setText(tr("Already set"));
    }

    // CACHE macro
    for (auto a: innerEditor->get_editor_cached()) {
        if (a.second.isEmpty()) {
            a.first->setText(tr("Not set"));
        } else {
            a.first->setText(tr("Already set"));
        }
    }
}

void DialogEditProfile::on_custom_edit_clicked() {
    C_EDIT_JSON_ALLOW_EMPTY(custom)
    editor_cache_updated_impl();
}

void DialogEditProfile::on_certificate_edit_clicked() {
    bool ok;
    auto txt = QInputDialog::getMultiLineText(this, tr("Certificate"), "", CACHE.certificate, &ok);
    if (ok) {
        CACHE.certificate = txt;
        editor_cache_updated_impl();
    }
}
