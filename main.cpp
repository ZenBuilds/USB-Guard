
/*EXTERNAL LIBRARIES*/
#include "mainwindow.h"
#include <QApplication>
#include "QTreeWidget"
#include "pthread.h"
#include "sstream"
#include <QDebug>
#include <QMenu>
#include <QAction>
#include <QObject>

/*INTERNAL LIBRARIES*/
#include "libs.h"
#include "disabler.h"

#define CLMN_DEVICE 0
#define CLMN_TYPE   1
#define CLMN_VID    2
#define CLMN_PID    3

libusb_device **devs;
libusb_context *context = NULL;
QList<QTreeWidgetItem *> items;
map<libusb_device*, QTreeWidgetItem*> dev_item_map;

void test_qt_tree(libusb_device **devs, QTreeWidget *treeWidget);
void arange_tree(QTreeWidget *&treeWidget, libusb_device **devs);
QString get_device_type(libusb_device *dev);
int hotplug_flush_UI(string op, libusb_device *dev);
void click_item(QTreeWidget* treeWidget);
void pop_menu(QPoint p, QTreeWidget* treeWidget);

void device_init(libusb_device **&devs);
int uninstall_device(libusb_device *dev);
int install_device(libusb_device *dev);
int get_device_list(libusb_device **&devs, libusb_context *&context);
void print_devices(libusb_device *dev);
int *get_vid_pid(libusb_device *dev);
libusb_device *get_device_by_vid_pid(libusb_device **devs, int vid, int pid);
libusb_device *get_device_by_vid_pid_2(libusb_device **devs, int vid, int pid, libusb_context *context);
//static int hotplug_callback(struct libusb_context *ctx, struct libusb_device *device,
//    libusb_hotplug_event event, void *user_data);

static int hotplug_callback(struct libusb_context *ctx,struct libusb_device *device,
                            libusb_hotplug_event event, void *user_data);
void *listen_hotplug(void *args) {
    while(1) {
        libusb_handle_events(context);
    }
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    //    MainWindow w;
    //    w.show();
    QTreeWidget *treeWidget = new QTreeWidget();
    int ret;
    libusb_device *dev;

    pthread_t listener;


    ret = libusb_init(&context);
    if (ret < 0) {
        cout << "failed to initialise libusb: " << libusb_error_name(ret) << endl;
        return EXIT_FAILURE;
    }
    get_device_list(devs,context);
    device_init(devs);

    libusb_hotplug_event hp_event   = static_cast<libusb_hotplug_event> (LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED | LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT);
    int hp_vid                      = LIBUSB_HOTPLUG_MATCH_ANY;
    int hp_pid                      = LIBUSB_HOTPLUG_MATCH_ANY;
    int hp_dev_class                = LIBUSB_HOTPLUG_MATCH_ANY;
    libusb_hotplug_flag hp_flag     = static_cast<libusb_hotplug_flag>(0);
    void *hp_user_data;
    libusb_hotplug_callback_handle cb_handle = NULL;

    int result_Register = -2;
    result_Register = libusb_hotplug_register_callback(context, hp_event, hp_flag, hp_vid, hp_pid, hp_dev_class, hotplug_callback, &hp_user_data, &cb_handle);
    if (result_Register != (int)LIBUSB_SUCCESS) {
        cout << "Resigter hotplug_callback failed";
        return -1;
    } else {
       cout << "Resigter hotplug_callback successfully";
    }

    ret = pthread_create(&listener, NULL, listen_hotplug, NULL);
    if(ret != 0) {
        cout << "Creating pthread failed" << endl;
    }

    test_qt_tree(devs, treeWidget);
    treeWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(treeWidget, SIGNAL(customContextMenuRequested(QPoint)), NULL, SLOT(pop_menu(QPoint, treeWidget)));
    
    //connect(disable_item, SIGNAL(triggered(bool)), this_item, SLOT(NULL));

//    while(1) {
//        libusb_handle_events(context);
//    }
    //libusb_handle_events_timeout(context, 0);
    a.exec();

    libusb_hotplug_deregister_callback(context, cb_handle);
    libusb_free_device_list(devs,1);
    libusb_exit(context);

    pthread_exit(NULL);
    return 0;
}

int get_device_list(libusb_device **&devs, libusb_context *&context)
{

    int ret;
    ssize_t i;
    ssize_t list;

    if (!libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG)) {
        //nonzero if the running library has the capability, 0 otherwise
        cout << "Hotplug capabilites are not supported on this platform" << endl;
        libusb_exit(NULL);
        return EXIT_FAILURE;
    }
    if (!libusb_has_capability(LIBUSB_CAP_SUPPORTS_DETACH_KERNEL_DRIVER)) {
        //nonzero if the running library has the capability, 0 otherwise
        cout << "Capabilites of detaching kernel driver are not supported on this platform" << endl;
        libusb_exit(NULL);
        return EXIT_FAILURE;
    }
    if (!libusb_has_capability(LIBUSB_CAP_HAS_HID_ACCESS)) {
        //nonzero if the running library has the capability, 0 otherwise
        cout << "Capabilites of detaching kernel driver are not supported on this platform" << endl;
        libusb_exit(NULL);
        return EXIT_FAILURE;
    }

    ret = libusb_get_device_list(context, &devs);
    if (ret < 0) {
        cout << "Libusb get device list error: " << libusb_error_name(ret) << endl;
        libusb_free_device_list(devs, 1);
        libusb_exit(context);
        return EXIT_FAILURE;
    }else{
        list = ret;
    }


    for (i = 0;i < list;i++) {

        cout << i << " ";
        //print_devices(devs[i]);
    }

}

void print_devices(libusb_device *dev)
{
    libusb_device_descriptor dev_desc;
    libusb_config_descriptor *config_desc;
    //const libusb_interface *interface;
    //const libusb_interface_descriptor *interface_desc;
    //const libusb_endpoint_descriptor *endpoint_desc;
    //libusb_device_handle *handle = NULL;
    int ret;

    ret = libusb_get_device_descriptor(dev, &dev_desc);
    if (ret < 0) {
        cout << "error in detting device descriptor" << libusb_error_name(ret) << endl;
        return;
    }

    cout << "Number of POS configs is " << dev_desc.bNumConfigurations << endl;
    cout << "Device Vendor id): " << dev_desc.idVendor << endl;
    cout << "Product ID: " << dev_desc.idProduct << endl;
    cout << "Class ID: " << dev_desc.bDeviceClass << endl;

    ret = libusb_get_config_descriptor(dev, 0, &config_desc);
    if (ret == LIBUSB_ERROR_NOT_FOUND) {
        /*0 on success
        LIBUSB_ERROR_NOT_FOUND if the configuration does not exist
        another LIBUSB_ERROR code on error*/
        cout << "The configuration does not exist" << endl;
    }else if(ret < 0){
        cout << "Error Code in libusb_get_config_descriptor: " << libusb_error_name(ret);
    }else{
        cout << "Interface: " << config_desc->bNumInterfaces << endl;
    }
    cout << "*********************************************************" << endl;
}

int *get_vid_pid(libusb_device *dev) {
    libusb_device_descriptor dev_desc;
    int ret;
    int vid_pid[2];

    ret = libusb_get_device_descriptor(dev, &dev_desc);
    if (ret < 0) {
        cout << "error in detting device descriptor" << libusb_error_name(ret) << endl;
        vid_pid[0] = EXIT_FAILURE;
        vid_pid[1] = EXIT_FAILURE;
        return vid_pid;
    }else{
        vid_pid[0] = dev_desc.idVendor;
        vid_pid[1] = dev_desc.idProduct;
        return vid_pid;
    }
}

int uninstall_device(libusb_device *dev) {
    int *vid_pid;
    int ret;

    vid_pid = get_vid_pid(dev);
    libusb_device_handle *handle = NULL;
    libusb_open(dev, &handle);
    ret = libusb_detach_kernel_driver(handle, 0);
    if (ret < 0) {
        cout << "Error in detach kernel driver to device " << dev << ": " << libusb_error_name(ret) <<endl;
        libusb_close(handle);
        return ret;
    }else{
        libusb_close(handle);
        return EXIT_SUCCESS;
    }
}

int install_device(libusb_device *dev) {
    int *vid_pid;
    int ret;

    vid_pid = get_vid_pid(dev);
    libusb_device_handle *handle = NULL;
    libusb_open(dev, &handle);
    ret = libusb_attach_kernel_driver(handle, 0);
    if (ret < 0) {
        cout << "Error in attach kernel driver to device " << dev << ": " << libusb_error_name(ret) <<endl;
        libusb_close(handle);
        return ret;
    }else{
        libusb_close(handle);
        return EXIT_SUCCESS;
    }
}

libusb_device *get_device_by_vid_pid(libusb_device **devs, int vid, int pid) {
    libusb_device_descriptor dev_desc;
    int ret;
    ssize_t i = 0;//count device

    while(devs[i]){
        ret = libusb_get_device_descriptor(devs[i], &dev_desc);
        if (ret < 0) {
            cout << "Error Code in libusb_get_config_descriptor of device " << i << " : " << libusb_error_name(ret);
            i++;
        }else {
            if (vid == dev_desc.idVendor && pid == dev_desc.idProduct) {
                cout << "correct";
                return devs[i];
            }else {
                i++;
            }
        }
    }

}

libusb_device *get_device_by_vid_pid_2(libusb_device **devs, int vid, int pid, libusb_context *context) {
    libusb_device_descriptor dev_desc;
    int ret;
    ssize_t i = 0;//count device

    ssize_t list;
    ret = libusb_get_device_list(context, &devs);
    if (ret < 0) {
        cout << "Libusb get device list error: " << libusb_error_name(ret) << endl;
        libusb_free_device_list(devs, 1);
        libusb_exit(context);

    }else{
        list = ret;
    }

    for(i; i < list; i++){
        ret = libusb_get_device_descriptor(devs[i], &dev_desc);
        if (ret < 0) {
            cout << "Error Code in libusb_get_config_descriptor of device " << i << " : " << libusb_error_name(ret);
            i++;
        }else {
            if (vid == dev_desc.idVendor && pid == dev_desc.idProduct) {
                cout << "correct";
                return devs[i];
            }else {
                i++;
            }
        }
    }
}

void device_init(libusb_device **&devs) {
    libusb_device_handle *handle;
    ssize_t i = 0;
    int ret;

    while(devs[i]){
        libusb_open(devs[i], &handle);
        ret = libusb_kernel_driver_active(handle, 0);
        switch (ret) {
        case 0:
            install_device(devs[i]);
            break;
        case 1:
            break;
        case LIBUSB_ERROR_NO_DEVICE:
            cout << "LIBUSB_ERROR_NO_DEVICE";
            break;
        case LIBUSB_ERROR_NOT_SUPPORTED:
            cout << "LIBUSB_ERROR_NOT_SUPPORTED";
            break;
        default:
            break;
        }
       // uninstall_device(devs[i]);
        libusb_close(handle);
        i++;
    }

}

static int hotplug_callback(struct libusb_context *ctx,struct libusb_device *device,
                            libusb_hotplug_event event, void *user_data) {
    if(event == LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED) {
        int *vid_pid;
        int ret;

        vid_pid = get_vid_pid(device);
        disabler dis;
        if(dis.is_device_disabled(vid_pid[0], vid_pid[1]) == DISABLED) {
            ret = uninstall_device(device);
            if(ret < 0) {
                return -1;
            }
        }
        //qDebug() << "123";
        hotplug_flush_UI("insert", device);
     }else if (event == LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT) {
        libusb_free_device_list(devs, 1);
        get_device_list(devs, context);
     }
    return EXIT_SUCCESS;
}

void test_qt_tree(libusb_device** devs, QTreeWidget *treeWidget) {

    treeWidget->setColumnCount(4);
    QStringList labels;
    labels.append("Device");
    labels.append("Type");
    labels.append("Vendor ID");
    labels.append("Product ID");
    treeWidget->setHeaderLabels(labels);
    treeWidget->setGeometry(0, 0, 1024, 768);
    treeWidget->setContextMenuPolicy(Qt::CustomContextMenu);

//    QList<QTreeWidgetItem *> items;
//    //treeWidget->insertTopLevelItems(0, items);
//    QTreeWidgetItem *item = new QTreeWidgetItem;
//    item->setText(0, "hello");
//    QTreeWidgetItem *c = new QTreeWidgetItem;
//    c->setText(0, "world");
//    item->addChild(c);
//    items.append(item);

//    QTreeWidgetItem *item2 = new QTreeWidgetItem;
//    item2->setText(0, "hello");
//    items.append(item2);


    arange_tree(treeWidget, devs);
    treeWidget->expandAll();// must be placed behind 'insertTopLevel'
    treeWidget->show();
}

void arange_tree(QTreeWidget *&treeWidget, libusb_device **devs) {
    /* for device returning NULL parent pointer: be parent
     * for device returning a non-NULL device, go get a parent
     * for device did not find the parent in UI, go to stack and match later
     */
    int i = 0;
    int *vid_pid;

    while(devs[i]) {
       // tmp_dev = libusb_get_parent(devs[i]);
        QTreeWidgetItem *tmp_item = new QTreeWidgetItem;
        vid_pid = get_vid_pid(devs[i]);

        int this_vid = vid_pid[0];//必须复制，否则数组内值会因为未知原因变
        int this_pid = vid_pid[1];
//        QString str = get_device_type(devs[i]);
//        qDebug() << str;
        tmp_item->setText(CLMN_DEVICE,  QString::fromStdString(to_string(i)));
        tmp_item->setText(CLMN_TYPE,    "Unresolved");
        tmp_item->setText(CLMN_VID,     QString::fromStdString(to_string(this_vid)));
        tmp_item->setText(CLMN_PID,     QString::fromStdString(to_string(this_pid)));

        if(libusb_get_parent(devs[i]) == NULL) {
            items.append(tmp_item);
        }
        /*build device-item map*/
        dev_item_map[devs[i]] = tmp_item;
        i++;
    }
    for(auto iter = dev_item_map.begin(); iter != dev_item_map.end(); iter++) {
        libusb_device *this_parent;
        this_parent = libusb_get_parent(iter->first);

        if(this_parent != NULL) {
            auto iter_parent = dev_item_map.begin();
            while(iter_parent != dev_item_map.end()) {
                if(iter_parent->first == this_parent) {
                    iter_parent->second->addChild(iter->second);
                }
                iter_parent++;
            }
        }

    }
    treeWidget->insertTopLevelItems(0, items);
    //click_item(treeWidget);
}


void click_item(QTreeWidget* treeWidget) {

    QTreeWidgetItem* this_item = treeWidget->currentItem();  //**获取当前被点击的节点
    if(this_item == NULL/* || this_item->parent() == NULL*/) {
        return;           //右键的位置在空白位置右击或者点击的是顶层item
    }

    libusb_device* this_dev;
    for(auto iter = dev_item_map.begin(); iter != dev_item_map.end(); iter++) {
        if(iter->second == this_item) {
            this_dev = iter->first;
        }
    }

    //创建action
    int *vid_pid;
    vid_pid = get_vid_pid(this_dev);
    disabler dis;
    if(dis.is_device_disabled(vid_pid[0], vid_pid[1]) == DISABLED) {
        QMenu *menu = new QMenu(treeWidget);
        QAction enable_item(QString::fromLocal8Bit("启用"), treeWidget);

        menu->addAction(&enable_item);
        menu->exec(QCursor::pos());
    }else {                                            //ENABLED
        libusb_device_handle **handle;
        libusb_open(this_dev, handle);

        if(libusb_kernel_driver_active(*handle, 0) == 0) { //not using
            QAction install_item(QString::fromLocal8Bit("安装"), treeWidget);
            QAction disable_item(QString::fromLocal8Bit("禁用"), treeWidget);
            QMenu *menu = new QMenu(treeWidget);

            menu->addAction(&install_item);
            menu->addAction(&disable_item);
            menu->exec(QCursor::pos());
        }else if(libusb_kernel_driver_active(*handle, 0) == 1) { //using
            QAction uninstall_item(QString::fromLocal8Bit("卸载"), treeWidget);
            QAction disable_item(QString::fromLocal8Bit("禁用"), treeWidget);
            QMenu *menu = new QMenu(treeWidget);
            //connect(disable_item, SIGNAL(triggered(bool)), this_item, SLOT(NULL));

            menu->addAction(&uninstall_item);
            menu->addAction(&disable_item);
            menu->exec(QCursor::pos());
        }
    }

//    connect(&deleteItem, SIGNAL(triggered()), this, SLOT(deleteItem()));
//    QPoint pos;
//    //创建菜单栏
//    QMenu menu(treeWidget);
//    menu.addAction(&deleteItem);
//    menu.exec(QCursor::pos());  //在当前鼠标位置显示
}

int hotplug_flush_UI(string op, libusb_device *dev) {
    QTreeWidgetItem* item = dev_item_map[dev];
    int *vid_pid;
    map<libusb_device*, QTreeWidgetItem*>::iterator parent_iter;

    if(op == "remove") {
        items.removeOne(item);
        dev_item_map.erase(dev);
        return EXIT_SUCCESS;
    }else if(op == "insert") {
        vid_pid = get_vid_pid(dev);
        int devs_count = 2;
        int this_vid = vid_pid[0];//必须复制，否则数组内值会因为未知原因溢出
        int this_pid = vid_pid[1];
        qDebug() << this_vid << this_pid;
        QString str = "new";
        qDebug() << str;
        while(devs[devs_count]) {
            devs_count++;
        }

        item->setText(CLMN_DEVICE,  QString::fromStdString(to_string(devs_count)));
        item->setText(CLMN_TYPE,    "Unresolved");
        item->setText(CLMN_VID,     QString::fromStdString(to_string(this_vid)));
        item->setText(CLMN_PID,     QString::fromStdString(to_string(this_pid)));
        dev_item_map[dev] = item;

        parent_iter = dev_item_map.find(libusb_get_parent(dev));
        parent_iter->second->insertChild(0,item);
        qDebug() << "insert successfully";
        return EXIT_SUCCESS;
    }
    return EXIT_FAILURE;
}

void pop_menu(QPoint p, QTreeWidget* treeWidget) {
    QMenu *menu = new QMenu("test", treeWidget);
    menu->move(123,123);
    menu->show();
}


QString get_device_type(libusb_device* dev) {
    libusb_device_descriptor *dev_desc;
    libusb_config_descriptor *config_desc;
    const libusb_interface *interface;
    //const libusb_interface_descriptor *interface_desc;
    //const libusb_endpoint_descriptor *endpoint_desc;
    //libusb_device_handle *handle = NULL;
    int ret;
    int dev_class, if_class;
    QString type;

    ret = libusb_get_device_descriptor(dev, dev_desc);
    if (ret < 0) {
        qDebug() << "error in detting device descriptor" << libusb_error_name(ret) << endl;
        return "get_device_desc failed";
    }
    dev_class = dev_desc->bDeviceClass;

    ret = libusb_get_config_descriptor(dev, 0, &config_desc);
//    interface = &config_desc->interface[0];
//    if_class = interface->altsetting[0].bInterfaceClass;
    if_class = 1;

    switch (dev_class) {
        case 0:
            switch(if_class){
                case 1:
                    return "Audio音频设备";
                    break;
                case 3:
                    return "HID(Human Interface Device)人机接口设备";
                    break;
                case 5:
                    return "Physical物理设备";
                    break;
                case 6:
                    return "Image图像设备";
                    break;
                case 7:
                    return "Printer打印机";
                    break;
                case 8:
                    return "Mass Storage 大容量存储";
                    break;
                case 10:
                    return "CDC-Data通信设备";
                    break;
                case 11:
                    return "Smart Card智能卡";
                    break;
                case 13:
                    return "Content Security内容安全设备";
                    break;
                case 14:
                    return "Video视频设备";
                    break;
                case 15:
                    return "Personal Healthcare个人健康设备";
                    break;
                case 16:
                    return "Audio/Video Devices声音/视频设备";
                    break;
                case 18:
                    return "USB Type-C Bridge Class";
                    break;
                case 224:
                    return "Wireless Controller无限控制器";
                    break;
                case 254:
                    return "Application Specific特定应用设备";
                    break;
                default:
                    return "Unrecognized Interface";
            }
                break;
        case 2:
            if(if_class == 2){
                return "Communications&CDC通信设备";
            }else {
                return "可能是Communications&CDC通信设备";
            }
            break;
        case 9:
            return "Hub集线器";
            break;
        case 17:
            return "Billboard Device Class广播牌设备";
            break;
        case 220:
            if(if_class == 220){
                return "Diagnostic Device";
            }else {
                return "可能是Diagnostic Device";
            }
            break;
        case 239:
            if(if_class == 239){
                return "Miscellaneous";
            }else {
                return "可能是Miscellaneous";
            }
            break;
        case 255:
            if(if_class == 255){
                return "Vendor Specific厂商自定义设备";
            }else {
                return "可能是Vendor Specific厂商自定义设备";
            }
            break;
        default:
            return "Unrecognized Device";
    }
    return "type";
}


