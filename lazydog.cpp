
#include "lazydog.h"
#include "ui_lazydog.h"

#include <QDebug>


LazyDog::LazyDog(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::LazyDog)
{
    ui->setupUi(this);

    //初始化系统托盘
    InitSystemTray();
    //初始化音频设备列表
    InitAudioDeviceList();
    //初始化进程窗口
    InitProcessTabview();
    //初始化监控窗口
    InitMonitorTabview();

    //链接槽函数
    QTimer *timer = new QTimer(this);
    timer->setInterval(1000); // 设置定时器间隔为1秒
    connect(timer, &QTimer::timeout, this, &LazyDog::auto_change_outaudiodevice);
    timer->start(); // 启动定时器
}

LazyDog::~LazyDog()
{
    delete ui;
}

//初始化系统托盘
void LazyDog::InitSystemTray()
{
    // 创建系统托盘图标
    trayIcon = new QSystemTrayIcon(QIcon(":/LD_64.ico"));
    trayIcon->setToolTip("LazyDogTools");
    trayIcon->show();

    // 创建托盘菜单
    QMenu *trayMenu = new QMenu();
    QAction *mainPageAction = new QAction("主界面");
    modeChooseAction = new QAction("进程模式");
    QAction *settingsAction = new QAction("设置");
    QAction *exitAction = new QAction("退出");

    // 将菜单项添加到托盘菜单
    trayMenu->addAction(mainPageAction);
    trayMenu->addAction(modeChooseAction);
    trayMenu->addAction(settingsAction);
    trayMenu->addSeparator(); // 添加分隔线
    trayMenu->addAction(exitAction);

    // 将托盘菜单设置为系统托盘图标的菜单
    trayIcon->setContextMenu(trayMenu);

    // 处理菜单项的点击事件
    connect(mainPageAction, &QAction::triggered, this, &LazyDog::tray_mainPage_triggered);
    connect(trayIcon, &QSystemTrayIcon::activated, this, &LazyDog::trayIcon_activated);
    connect(modeChooseAction, &QAction::triggered, this, &LazyDog::tray_modeChoose_triggered);
    connect(settingsAction, &QAction::triggered, this, &LazyDog::tray_settings_triggered);
    connect(exitAction, &QAction::triggered, this, &LazyDog::tray_exit_triggered);

    trayIcon->showMessage("程序启动成功", "欢迎使用，您的工具已准备就绪", QSystemTrayIcon::Information, 5000);
}

//初始化进程窗口
void LazyDog::InitProcessTabview()
{
    //设置选中时为整行选中
    ui->tableView_process->setSelectionBehavior(QAbstractItemView::SelectRows);
    //设置表格的单元为只读属性，即不能编辑
    ui->tableView_process->setEditTriggers(QAbstractItemView::NoEditTriggers);
    //根据内容自动调整列宽、行高
    ui->tableView_process->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->tableView_process->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    // 隐藏垂直表头，即隐藏行号
    ui->tableView_process->verticalHeader()->setVisible(false);

    // 更新进程任务列表
    taskmonitor = new TaskMonitor();
    RenewProcessTabview();
    // 重新调整第1列宽度
    ui->tableView_process->horizontalHeader()->setSectionResizeMode(0,QHeaderView::ResizeToContents);
}

//初始化监控窗口
void LazyDog::InitMonitorTabview()
{
    //设置选中时为整行选中
    ui->tableView_monitor->setSelectionBehavior(QAbstractItemView::SelectRows);
    //设置表格的单元为只读属性，即不能编辑
    ui->tableView_monitor->setEditTriggers(QAbstractItemView::NoEditTriggers);
    //根据内容自动调整列宽、行高
    ui->tableView_monitor->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->tableView_monitor->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    // 隐藏垂直表头，即隐藏行号
    ui->tableView_monitor->verticalHeader()->setVisible(false);

    // 读取配置文件
    bindlist = LoadBindListFromXml();
    // 初始化hashmap
    int index = 0;
    BindInfo infobuf;
    for (;index < bindlist.length(); index++)
    {
        infobuf = bindlist.at(index);
        bindmap[infobuf.exepath] = index;
    }
    ShowDebugText(D_Info, "读取绑定信息配置文件完成");

    // 更新进程任务列表
    RenewMonitorTabview();
}

//初始化音频设备列表
void LazyDog::InitAudioDeviceList()
{
    // 获取音频输出列表
    audiomanager = new AudioManager();
    outaudiodevicelist = audiomanager->GetOutAudioDeviceList();
    currentbindinfo.deviceid = audiomanager->GetDefaultAudioDevice();

    // 使用迭代器遍历QMap
    AudioDeviceList::const_iterator outputiter;
    for (outputiter = outaudiodevicelist.constBegin(); outputiter != outaudiodevicelist.constEnd(); ++outputiter) {
        // 设置关联选项
        ui->comboBox_add->addItem(outputiter.key());
        ui->comboBox_change->addItem(outputiter.key());
    }
}

//更新进程任务列表视图
void LazyDog::RenewProcessTabview()
{
    // 清除原来的数据
    processlist.clear();
    processlist = taskmonitor->GetTaskList();

    // 创建数据模型
    QStandardItemModel *model = new QStandardItemModel();
    model->setColumnCount(2);
    model->setHorizontalHeaderItem(0, new QStandardItem("PID"));
    model->setHorizontalHeaderItem(1, new QStandardItem("进程"));
    int row;
    ProcessList::const_iterator iter;
    for(iter = processlist.constBegin(), row = 0; iter != processlist.constEnd(); ++iter, ++row)
    {
        qDebug() << iter->pid << "|" << iter->name << "|" << iter->path;

        // 添加数据
        QList<QStandardItem*> Item;
        Item.append(new QStandardItem(QString::number(iter->pid)));
        Item.append(new QStandardItem(iter->name));
        model->appendRow(Item);

        // 设置图标
        SHFILEINFO shfi;
        memset(&shfi, 0, sizeof(SHFILEINFO));
        // 使用SHGetFileInfo函数获取EXE文件的图标
        if (SHGetFileInfo(reinterpret_cast<const wchar_t*>(iter->path.utf16()), 0, &shfi, sizeof(SHFILEINFO), SHGFI_ICON | SHGFI_SMALLICON))
        {
            QIcon exeFileIcon = QIcon(QPixmap::fromImage(QImage::fromHICON(shfi.hIcon)));
            model->setData(model->index(row, 1), exeFileIcon, Qt::DecorationRole);
        }

    }

    // 将数据模型设置到QTableView中
    ui->tableView_process->setModel(model);
    ui->tableView_process->update();
//    ui->debugBrowser->append("更新进程列表完成，进程总数：" + QString::number(processlist.length()));
    ShowDebugText(D_Info, "更新进程列表完成，进程总数：" + QString::number(processlist.length()));
}

//更新监控列表视图
void LazyDog::RenewMonitorTabview()
{
    // 创建数据模型
    QStandardItemModel *model = new QStandardItemModel();
    model->setColumnCount(2);
    model->setHorizontalHeaderItem(0, new QStandardItem("进程"));
    model->setHorizontalHeaderItem(1, new QStandardItem("关联设备"));
    int row;
    BindList::const_iterator iter;
    for(iter = bindlist.constBegin(), row = 0; iter != bindlist.constEnd(); ++iter, ++row)
    {
        // 添加数据
        QList<QStandardItem*> Item;
        Item.append(new QStandardItem(iter->taskname));
        Item.append(new QStandardItem(iter->devicename));
        model->appendRow(Item);

        // 设置图标
        SHFILEINFO shfi;
        memset(&shfi, 0, sizeof(SHFILEINFO));
        // 使用SHGetFileInfo函数获取EXE文件的图标
        if (SHGetFileInfo(reinterpret_cast<const wchar_t*>(iter->exepath.utf16()), 0, &shfi, sizeof(SHFILEINFO), SHGFI_ICON | SHGFI_SMALLICON))
        {
            QIcon exeFileIcon = QIcon(QPixmap::fromImage(QImage::fromHICON(shfi.hIcon)));
            model->setData(model->index(row, 0), exeFileIcon, Qt::DecorationRole);
        }

    }

    // 将数据模型设置到QTableView中
    ui->tableView_monitor->setModel(model);
    ui->tableView_monitor->update();
}

/**
 * @brief LazyDog::ShowDebugText
 * @param level debug等级
 * @param text  debug信息内容
 */
void LazyDog::ShowDebugText(QString level, QString text)
{
    QDateTime dateTime= QDateTime::currentDateTime();//获取系统当前的时间
    QString timeStr = dateTime.toString("yyyy-MM-dd hh:mm:ss");//格式化时间
    QString buffer = QString("%1 [%2] %3").arg(timeStr).arg(level.at(0)).arg(text);
    ui->debugBrowser->append(buffer);
    // 将滚动条定位到底部 --- 自动刷新
    ui->debugBrowser->verticalScrollBar()->setValue(ui->debugBrowser->verticalScrollBar()->maximum());
}

// 将bindlist保存至xml文件，文件名在custom.h中
void LazyDog::SaveBindListToXml()
{
    QFile file(bindfile);
    if (file.open(QIODevice::WriteOnly)) {
        QXmlStreamWriter xmlWriter(&file);
        xmlWriter.setAutoFormatting(true);
        xmlWriter.writeStartDocument();
        xmlWriter.writeStartElement("BindList");

        for (const BindInfo& bindInfo : bindlist) {
            xmlWriter.writeStartElement("BindInfo");
            xmlWriter.writeTextElement("TaskName", bindInfo.taskname);
            xmlWriter.writeTextElement("ExePath", bindInfo.exepath);
            xmlWriter.writeTextElement("DeviceName", bindInfo.devicename);
            xmlWriter.writeTextElement("DeviceId", bindInfo.deviceid);
            xmlWriter.writeEndElement();
        }

        xmlWriter.writeEndElement();
        xmlWriter.writeEndDocument();
        file.close();
    }
}

//从文件中读取bindlist的配置信息
QList<BindInfo> LazyDog::LoadBindListFromXml()
{
    BindList bindList;
    QFile file(bindfile);
    if (file.open(QIODevice::ReadOnly)) {
        QXmlStreamReader xmlReader(&file);

        while (!xmlReader.atEnd()) {
            if (xmlReader.isStartElement() && xmlReader.name().toString() == "BindInfo") {
                BindInfo bindInfo;
                while (!(xmlReader.isEndElement() && xmlReader.name().toString() == "BindInfo")) {
                    xmlReader.readNext();
                    if (xmlReader.isStartElement()) {
                        QString elementName = xmlReader.name().toString();
                        if (elementName == "TaskName") {
                            bindInfo.taskname = xmlReader.readElementText();
                        } else if (elementName == "ExePath") {
                            bindInfo.exepath = xmlReader.readElementText();
                        } else if (elementName == "DeviceName") {
                            bindInfo.devicename = xmlReader.readElementText();
                        } else if (elementName == "DeviceId") {
                            bindInfo.deviceid = xmlReader.readElementText();
                        }
                    }
                }
                bindList.append(bindInfo);
            } else {
                xmlReader.readNext();
            }
        }

        file.close();
    }
    return bindList;
}

//重写关闭信号
void LazyDog::closeEvent(QCloseEvent *event)
{
    this->hide();
    event->ignore();
}

//槽函数：刷新任务列表
void LazyDog::on_pushButton_renew_clicked()
{
    // 更新进程任务列表
    RenewProcessTabview();
}

//槽函数：添加绑定
void LazyDog::on_pushButton_add_clicked()
{
    // 获取选择内容
    QString audiodevicename = ui->comboBox_add->currentText();
    int tabviewindex = ui->tableView_process->currentIndex().row();

    // 合法性判断
    if (tabviewindex < 0 || tabviewindex >= processlist.length())
    {
        ShowDebugText(D_Error, "未选择任务进程，请选择后再进行绑定");
        return;
    }
    if (bindmap.contains(processlist.at(tabviewindex).path))
    {
        ShowDebugText(D_Error, "该任务已在监控列表中");
        return;
    }

    // 绑定
    BindInfo infobuf;
    infobuf.devicename = audiodevicename;
    infobuf.deviceid = outaudiodevicelist[audiodevicename];
    infobuf.exepath = processlist.at(tabviewindex).path;
    infobuf.taskname = processlist.at(tabviewindex).name;
    bindlist.append(infobuf);
    bindmap[infobuf.exepath] = bindlist.length() - 1;

    RenewMonitorTabview();
    ShowDebugText(D_Info, "绑定成功, 信息如下:");
    ShowDebugText(D_Info, "进程ID: " + QString::number(processlist.at(tabviewindex).pid) + ", 进程名: " + infobuf.taskname);
    ShowDebugText(D_Info, "绑定设备: " + audiodevicename);
    SaveBindListToXml();
}

//槽函数：更改绑定
void LazyDog::on_pushButton_change_clicked()
{
    // 获取选择内容
    QString audiodevicename = ui->comboBox_change->currentText();
    int tabviewindex = ui->tableView_monitor->currentIndex().row();

    // 合法性判断
    if (tabviewindex < 0 || tabviewindex >= processlist.length())
    {
        ShowDebugText(D_Error, "未选择任务进程，请选择后再进行更改");
        return;
    }

    // 更改
    BindInfo infobuf;
    infobuf.devicename = audiodevicename;
    infobuf.deviceid = outaudiodevicelist[audiodevicename];
    infobuf.exepath = bindlist.at(tabviewindex).exepath;
    infobuf.taskname = bindlist.at(tabviewindex).taskname;
    bindlist[tabviewindex] = infobuf;

    RenewMonitorTabview();
    ShowDebugText(D_Info,infobuf.taskname + " 已更改绑定设备：" + infobuf.devicename);
    SaveBindListToXml();
}

//槽函数：取消绑定
void LazyDog::on_pushButton_delete_clicked()
{
    // 获取选择内容
    int tabviewindex = ui->tableView_monitor->currentIndex().row();

    // 合法性判断
    if (tabviewindex < 0 || tabviewindex >= processlist.length())
    {
        ShowDebugText(D_Error, "未选择任务进程，请选择后再进行更改");
        return;
    }

    // 删除list
    QString deltaskname = bindlist.at(tabviewindex).taskname;
    QString delexepath = bindlist.at(tabviewindex).exepath;
    bindlist.removeAt(tabviewindex);

    // 更新hashmap
    bindmap.remove(delexepath);
    int index = tabviewindex;
    BindInfo infobuf;
    for (;index < bindlist.length(); index++)
    {
        infobuf = bindlist.at(index);
        bindmap[infobuf.exepath] = index;
    }

    RenewMonitorTabview();
    ShowDebugText(D_Info,deltaskname + " 已取消绑定设备");
    SaveBindListToXml();
}

//槽函数：自动任务
void LazyDog::auto_change_outaudiodevice()
{
    // 获取当前进程状态
    ProcessList processlistbuf = taskmonitor->GetTaskList();
    ProcessList::const_iterator iter;
    // 逆序匹配
    for (iter=--processlistbuf.end(); iter!=processlistbuf.begin(); iter--)
    {
        // 检查是否匹配成功
        if (bindmap.contains(iter->path))
        {
            int index = bindmap[iter->path];
            BindInfo infobuf = bindlist.at(index);
            // 检查是否需要切换设备,以免音频卡顿
            if (currentbindinfo.deviceid == infobuf.deviceid) return;
            audiomanager->SetOutAudioDevice(infobuf.deviceid); // 切换设备
            qDebug()<< "切换设备" << infobuf.deviceid;
            currentbindinfo = infobuf;
            QString buffer = QString("检测到进程%1，已切换至设备%2").arg(infobuf.taskname).arg(infobuf.devicename);
            ShowDebugText(D_Info, buffer);
            trayIcon->showMessage("切换设备", buffer, QSystemTrayIcon::Information, 5000); // 5000毫秒（5秒）后关闭通知
            return;
        }
    }
}

//托盘---双击
void LazyDog::trayIcon_activated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::DoubleClick)
    {
        // 双击托盘图标时显示主窗口
        if (this->isHidden())
            this->show();
        this->activateWindow();
    }
}

//托盘---主界面
void LazyDog::tray_mainPage_triggered()
{
    if (this->isHidden())
        this->show();
    ui->mainWidget->setCurrentWidget(ui->tab_process);
    this->activateWindow();
}

//托盘---选择模式
void LazyDog::tray_modeChoose_triggered()
{
    QString modeText = modeChooseAction->text()=="进程模式" ? "窗口模式" : "进程模式";
    modeChooseAction->setText(modeText);
}

//托盘---设置
void LazyDog::tray_settings_triggered()
{
    ui->mainWidget->setCurrentWidget(ui->tab_seting);
    if (this->isHidden())
        this->show();
}

//托盘---退出
void LazyDog::tray_exit_triggered()
{
    QApplication::quit();
}
