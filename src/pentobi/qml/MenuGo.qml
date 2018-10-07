//-----------------------------------------------------------------------------
/** @file pentobi/qml/MenuGo.qml
    @author Markus Enzenberger
    @copyright GNU General Public License version 3 or later */
//-----------------------------------------------------------------------------

import "." as Pentobi

Pentobi.Menu {
    title: addMnemonic(qsTr("Go"),
                       //: Mnemonic for menu Go. Leave empty for no mnemonic.
                       qsTr("O"))

    Pentobi.MenuItem {
        action: actionGotoMove
        text: addMnemonic(action.text,
                          //: Mnemonic for menu item Go/Move Number. Leave empty for no mnemonic.
                          qsTr("N"))
    }
    Pentobi.MenuItem {
        action: actionBackToMainVar
        text: addMnemonic(actionBackToMainVar.text,
                          //: Mnemonic for menu item Go/Main Variation. Leave empty for no mnemonic.
                          qsTr("M"))
    }
    Pentobi.MenuItem {
        action: actionBeginningOfBranch
        text: addMnemonic(actionBeginningOfBranch.text,
                          //: Mnemonic for menu item Beginning Of Branch. Leave empty for no mnemonic.
                          qsTr("B"))
    }
    Pentobi.MenuSeparator { }
    Pentobi.MenuItem {
        action: actionNextComment
        text: addMnemonic(action.text,
                          //: Mnemonic for menu item Next Comment. Leave empty for no mnemonic.
                          qsTr("C"))
    }
}
