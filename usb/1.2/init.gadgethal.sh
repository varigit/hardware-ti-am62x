#!/vendor/bin/sh

#
# modify config/usb_gadget/ permission
#
echo "check acces"

if [ -d /config/usb_gadget ]; then
    echo "change acces"
    chown -hR system.system /config/usb_gadget/
fi
