#include <fcntl.h>
#include "check_usb.h"

#define SYS_FS_CURRENT          "/sys/class/power_supply/battery/BatteryAverageCurrent"
#define SYS_FS_CHARGE_ENABLED   "/sys/class/power_supply/battery/charging_enabled"
#define SYS_FS_USB_ONLINE       "/sys/class/power_supply/usb/online"
#define SYS_FS_AC_ONLINE       "/sys/class/power_supply/ac/online"
#define BUF_SIZE   32

int16 get_battery_current(void)
{
    int16 fd = 0;
    char buf[BUF_SIZE];
    int16 current = 0;

    fd = open(SYS_FS_CURRENT, O_RDONLY);
    if (fd < 0) {
        ALOGE("%s:%d, error:%s\n", __FUNCTION__, __LINE__, strerror(errno));
        return -1;
    }

    read(fd, buf, BUF_SIZE);
    close(fd);

    current = atoi(buf);
	ALOGE("akm_log get_battery_current = %d\n",current);
    return current;
}

int16 check_usb_online(void)
{
    int16 fd = 0;
    char buf[BUF_SIZE];
    int16 status = 0;
    int usb_ret = 0;
    int ac_ret = 0;
    int ret = 0;

    fd = open(SYS_FS_USB_ONLINE, O_RDONLY);
    if (fd < 0) {
        ALOGE("%s:%d, error:%s\n", __FUNCTION__, __LINE__, strerror(errno));
        return -1;
    }

    read(fd, buf, sizeof(int16));
    close(fd);

    status = atoi(buf);

    usb_ret = (buf[0] == '1') ? 1 : 0;
    //ALOGE("akm_log Test usb online : %d, usb_ret = %c", status, buf[0]);

    fd = open(SYS_FS_AC_ONLINE, O_RDONLY);
    if (fd < 0) {
        ALOGE("%s:%d, error:%s\n", __FUNCTION__, __LINE__, strerror(errno));
        return -1;
    }

    read(fd, buf, sizeof(int16));
    close(fd);

    status = atoi(buf);

    ac_ret = (buf[0] == '1') ? 1 : 0;
    //ALOGE("akm_log Test ac online : %d, ac_ret = %c", status, buf[0]);

    ret = usb_ret | ac_ret;

    return ret;
}

int16 check_usb_charging(void)
{
    int16 fd = 0;
    char buf[BUF_SIZE];
    int16 status = 0;
    int ret = 0;

    fd = open(SYS_FS_CHARGE_ENABLED, O_RDONLY);
    if (fd < 0) {
        ALOGE("%s:%d, error:%s\n", __FUNCTION__, __LINE__, strerror(errno));
        return -1;
    }

    read(fd, buf, sizeof(int16));
    close(fd);

    status = atoi(buf);

    ret = (buf[0] == '1') ? 1 : 0;
    //ALOGE("AKM Test usb charging: %d, ret = %c", status, buf[0]);

    return ret;
}
