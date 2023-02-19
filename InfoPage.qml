// -*- javascript -*-

import QtQuick 2.0
import Sailfish.Silica 1.0

Page {
    backNavigation: false // wanted to drop button, dropped "scrolling" too...
    Column {
//        anchors.fill: parent
        width: parent.width
//        height: parent.height
        anchors.verticalCenter: parent.verticalCenter
//        anchors.centerIn: parent
        spacing: Theme.fontSizeSmall / 2
        Repeater {
            model: [ "Mad Developer 2023",

"when sshd is started from this application \
'defaultuser' and 'nemo' can be used \
interchangeably as usernames to log in.",

"more usernames can be added to
'/usr/share/mad-developer/loginnames'.",

"when entering via usb (rndis0) using address \
192.168.2.15 the host key is the same in all \
devices for the sshd connections:",

"SHA256:qqqqqI3jMdIvxbHyO6DY/iF0c5KZunrbwp7v405vW4Q.",

"login password can be set in
Settings -> Developer tools.",

"the program launchers in this application use \
`pkexec` to execute inetsshd and exitsshconns. \
sometimes it does not work. in those cases opening \
'Settings' in App Grid may help.",

"(inet)sshd exits when this application is closed. \
current ssh connections are not disconnected." ]
            Label {
                width: parent.width
                //anchors.centerIn: parent
                //anchors.horizontalCenter: parent.horizontalCenter
                text: modelData
                wrapMode: Text.WordWrap
                //readOnly: true
                font.pixelSize: Theme.fontSizeSmall
                horizontalAlignment: TextEdit.AlignHCenter
            }
         }
    }
    MouseArea {
        anchors.fill: parent
        onClicked: pageStack.pop()
    }
}
