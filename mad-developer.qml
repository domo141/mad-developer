// -*- javascript -*-

import QtQuick 2.0
import Sailfish.Silica 1.0
import io.thp.pyotherside 1.2

// SPDX-License-Identifier: BSD 2-Clause "Simplified" License

ApplicationWindow {
    id: aw
    property Python python
    property string usbip_text: " "
    property string wlanip_text: " "
    property string sshd_clbl: " " // sshd.checked? " ¤": " ·"
    function set_sshd_clbl(checked) { sshd_clbl = checked ? " ¤" : " ·" }

    cover: Component {
        CoverBackground {
            Label {
                text: "Mad Developer"
                anchors.right: parent.right
                anchors.rightMargin: Theme.paddingLarge
                anchors.top: parent.top
                color: Theme.highlightColor
                font.pixelSize: Theme.fontSizeSmall
            }
            Column {
                anchors.centerIn: parent
                Label {
                    text: usbip_text
                    font.pixelSize: Theme.fontSizeSmall
                }
                Label {
                    text: wlanip_text
                    font.pixelSize: Theme.fontSizeSmall
                }
                Label {
                    text: sshd_clbl
                    font.pixelSize: Theme.fontSizeSmall
                }
            }
            CoverActionList {
                CoverAction {
                    iconSource: "image://theme/icon-cover-refresh"
                    onTriggered: python.usb_wlan_ipv4s()
                }
            }
        }
    }
    initialPage: Component {
        Page {
            PageHeader {
                id: header
                title: "Mad Developer"
            }
/*
            GlassItem {
                anchors { left: parent.left; leftMargin: -(width/2) }
                MouseArea {
                    anchors.fill: parent
                    onClicked: pageStack.push("InfoPage.qml")
                }
            }
*/
/*
            Button {
//                icon.source: "image://theme/icon-m-about"
                anchors.verticalCenter: header.verticalCenter
//                width: 96
                text: '[i]'
                preferredWidth: Theme.buttonWidthTiny
                onClicked: pageStack.push("InfoPage.qml")
            }
*/
            // note: similar case below, w/ BackgroundItem (that done later)
            Label {
                anchors.verticalCenter: header.verticalCenter
                //anchors.left: parent.left
                //anchors.leftMargin: Theme.horizontalPageMargin / 2
                x: Theme.horizontalPageMargin / 2
                text: '[i]'
                opacity: 0.4
                MouseArea {
                    //anchors.fill: parent
                    anchors.top: parent.top
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    width: parent.width + parent.font.pixelSize
                    onClicked: pageStack.push("InfoPage.qml")
                    //Component.onCompleted: { console.log(width) }
                }
            }
            MouseArea {
                width: parent.width
                anchors.top: header.bottom
                anchors.bottom: updt.bottom
                onClicked: python.usb_wlan_ipv4s()
            }
            Label {
                id: tt
                anchors.top: header.bottom
                anchors.left: parent.left
                anchors.leftMargin: Theme.horizontalPageMargin / 2
                text: "Network configuration. Tap to refresh."
            }
            Grid {
                id: grid
                width: parent.width
                anchors.top: tt.bottom
                anchors.topMargin: tt.font.pixelSize / 2
                anchors.left: parent.left
                anchors.leftMargin: Theme.horizontalPageMargin
                columns: 3
                spacing: 10
                Label { id: usb;   text: "rndis0:"; width: parent.width / 4 }
                Label { id: usbip; text: usbip_text; width: parent.width / 2 }
                Label { id: usbnw; text: " " }
                Label { id: wlan;   text: "wlan0:"; width: parent.width / 4 }
                Label { id: wlanip; text: wlanip_text; width: parent.width / 2 }
                Label { id: wlannw; text: " " }
            }
            Label {
                id: updt
                anchors.top: grid.bottom
                anchors.topMargin: font.pixelSize / 2
                anchors.right: header.right
                anchors.rightMargin: font.pixelSize
                text: " "
                color: Theme.secondaryColor
            }
            TextSwitch {
                id: sshd
                anchors.top: updt.bottom
                anchors.topMargin: updt.font.pixelSize / 2
                anchors.left: parent.left
                width: parent.width / 2
                text: "sshd"
                automaticCheck: false
                onClicked: {
                    busy = true
                    python.call('start_inetsshd', [checked],
                         function(result) {
                             checked = result[0]
                             msgt.text = result[1]
                             set_sshd_clbl(checked)
                             busy = false
                         })
                }
            }
            Label {
                id: essh
                anchors.top: sshd.bottom
                anchors.topMargin: font.pixelSize / 2
                anchors.left: parent.left
                anchors.leftMargin: Theme.horizontalPageMargin
                text: "Exit all inbound ssh connections:"
                color: Theme.secondaryColor
            }
            BackgroundItem {
                anchors.verticalCenter: essh.verticalCenter
                anchors.right: header.right
                anchors.rightMargin: header.rightMargin * 1.5 // XXX
                width: bii.width
                height: bii.height
                Rectangle {
                    id: birt
                    anchors.fill: parent
                    color: Theme.highlightColor
                    opacity: 0.5
                    visible: false
                    radius: 10
                }
                Timer {
                    id: bit
                    interval: 100
                    onTriggered: birt.visible = false
                }
                Icon {
                    id: bii
                    source: "image://theme/icon-splus-clear"
                }
                onClicked: {
                    birt.visible = true
                    bit.start()
                    python.run_exitsshconns()
                }
            }
            Label {
                id: msgt
                anchors.bottom: parent.bottom
                anchors.bottomMargin: font.pixelSize / 2
                horizontalAlignment: TextEdit.AlignHCenter
                width: parent.width
                //text: "22:22:22: connection from 222.222.222.222"
                //text: "22:22:22: connection from 192.168.2.15 (usb)"
                text: "messages will appear here"
                font.pixelSize: Theme.fontSizeSmall
                wrapMode: Text.WordWrap
                MouseArea {
                    anchors.fill: parent
                    onClicked: msgt.text = ""
                }
            }
            Python {
                id: python
                Component.onCompleted: {
                    //console.log("where does this message go ?")
                    addImportPath(Qt.resolvedUrl('.'))
                    setHandler('msg', function(running, msg) {
                        msgt.text = msg
                        sshd.checked = running
                        set_sshd_clbl(running)
                    })
                    importNames('mad-developer',
                                ['usb_wlan_ipv4s', 'start_inetsshd',
                                 'run_exitsshconns'], function() {})
                    python.usb_wlan_ipv4s()
                    aw.python = python
                    set_sshd_clbl(false)
                }
                function usb_wlan_ipv4s() {
                    call('usb_wlan_ipv4s', [],
                         function(result) {
                             usbip_text = result[0];  usbnw.text = result[1]
                             wlanip_text = result[2]; wlannw.text = result[3]
                             wlan.text = result[4];   updt.text = result[5]
                         })
                }
                function run_exitsshconns() {
                    call('run_exitsshconns', [],
                         function(result) { msgt.text = result })
                }
            }
        }
    }
}
