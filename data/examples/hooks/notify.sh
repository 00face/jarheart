#!/bin/sh
# Example Jarheart hook script: Send desktop notifications on state changes
# Install to ~/.config/jarheart/hooks/notify.sh and chmod +x

case "$1" in
    period-changed)
        notify-send -a "Jarheart" -i "display" "Display Transition" "Sun elevation period changed from $2 to $3"
        ;;
    status-changed)
        notify-send -a "Jarheart" -i "display" "Jarheart Status" "Adjustment status changed to $2"
        ;;
esac
