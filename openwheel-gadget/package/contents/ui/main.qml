import QtQuick 2.15
import org.kde.kirigami 2.20 as Kirigami
import org.kde.plasma.plasmoid 2.0

Kirigami.InlineMessage {
    id: placeholder
    type: Kirigami.MessageType.Information
    text: i18n("OpenWheel gadget placeholder")
    visible: true
}
