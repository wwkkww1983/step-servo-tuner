#include "form.h"
#include "ui_form.h"

#include "command.h"
#include "configdialog.h"

#include <QMessageBox>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>

#include <QStandardItemModel>
#include <QMouseEvent>

#include <QtCore/QDebug>

Form::Form(QWidget *parent) :
    QWidget(parent),row(0),
    index(0), position(0),
    jmp_from(0), jmp_to(0),
    select_line(-1),
    ui(new Ui::Form)
{
    ui->setupUi(this);
    config = new ConfigDialog;

    int level = config->configs().elecLevel;
    int circle = config->configs().circleLen;
    int div[] = {1,2,4,8,16,32,64,128,256};
    deviceId = config->configs().deviceId;
    convert(id, deviceId, 2);
    param = 200 * div[level] / circle;
    //qDebug() << QString("param %1 pos %2").arg(param).arg(position);

    beta = 0.4 * div[level] / circle; // 200 * 100 / 50000

    int maxSpd = 100 / beta;
    ui->setRunSpd->setMaximum(maxSpd);
    //qDebug() << "param = " << param;
    initUI();
    initConnect();
    initModel();

    cmd_list = new QList<QByteArray>;
    //lines = new QList<CommandLine>;

    spd_show(ui->setRunSpd->value());

}

Form::~Form()
{
    delete ui;
    delete model;
    delete config;
    delete cmd_list;
}

void Form::about()
{
    QMessageBox::about(this, tr("控制器应用程序"),tr("V1.0 版应用程序"));
}

void Form::initUI()
{
    int countIn = config->configs().countIn;
    ui->jumpParam->setMaximum(countIn);
    ui->inputParam->setMaximum(countIn);

    int maxN = config->configs().maxN;
    int maxP = config->configs().maxP;
    ui->absMoveDistance->setMaximum(maxP);
    ui->absMoveDistance->setMinimum(maxN);
    ui->relMoveDistance->setMaximum(maxP);
    ui->relMoveDistance->setMinimum(maxN);
}

void Form::initConnect()
{
    //connect(ford_timer, SIGNAL(timeout()), this, SLOT(forward()));
    connect(ui->setRunSpd, SIGNAL(valueChanged(int)), this, SLOT(spd_show(int)));
    connect(ui->tableView, SIGNAL(doubleClicked(QModelIndex)), this, SLOT(tableDoubleClick(QModelIndex)));
    connect(ui->tableView, SIGNAL(clicked(QModelIndex)), this, SLOT(tableClick(QModelIndex)));
}

void Form::initModel()
{
    model = new QStandardItemModel(0, 2, this);
    model->setHeaderData(0, Qt::Horizontal, tr("指令类型"));
    model->setHeaderData(1, Qt::Horizontal, tr("指令内容"));

    ui->tableView->setModel(model);
    QHeaderView *headerView = ui->tableView->horizontalHeader();
    headerView->setStretchLastSection(true);

}

void Form::dragEnterEvent(QDragEnterEvent *event)
{

}

void Form::dropEvent(QDropEvent *event)
{

}

bool Form::saveProgFile(QString fileName) const
{
    QFile saveFile(fileName);
    if(!saveFile.open(QIODevice::WriteOnly)) {
        //qDebug() << tr("打开文件失败");
        return false;
    }
    QJsonObject json;
    QJsonArray lineArray;

    foreach (const CommandLine line, lines) {
        QJsonObject lineObject;
        line.write(lineObject);
        lineArray.append(lineObject);
    }
    json["list"] = lineArray;
    QJsonDocument saveDoc(json);
    saveFile.write(saveDoc.toJson());

    return true;
}

bool Form::loadProgFile(QString fileName)
{
    QFile loadFile(fileName);
    if(!loadFile.open(QIODevice::ReadOnly)) {
        //qDebug() << tr("打开文件失败");
        return false;
    }
    QByteArray saveData = loadFile.readAll();
    QJsonDocument loadDoc(QJsonDocument::fromJson(saveData));

    lines.clear();

    QJsonObject json = loadDoc.object();
    QJsonArray list = json["list"].toArray();
    for(int i=0; i < list.size(); ++i) {
        QJsonObject lineObject = list[i].toObject();
        CommandLine line;
        line.read(lineObject);
        lines.append(line);
    }

    //更新程序模型
    update_cmd();

    return true;
}

void Form::update_cmd()
{
    cmd_list->clear();
    moves.clear();

    model->removeRows(0, row, QModelIndex());
    ui->tableView->setModel(model);
    row = 0;
    index = 0;
    position = 0;

    for(row; row < lines.size(); row++) {
        CommandLine line = lines.at(row);
        model->insertRow(row, QModelIndex());
        model->setData(model->index(row, 0), line.type());
        model->setData(model->index(row,1), line.content());

        cmd_list->append(line.data());
    }
    ui->tableView->setModel(model);
}

void Form::on_absAddBtn_clicked()
{

    // Command cmd(data, type) 数据和类型内存创建指令数据结构
    position = ui->absMoveDistance->value();
    int val = position*param;

    moves.append(0);
    quint8 qpos[4];
    convert(qpos, position * param, 4);

    //qDebug() << QString("plus %1 pos %2").arg(position*param).arg(position);

    quint8 qPos[10] = {id[0],id[1],ABS_MOVE_CMD,0x01,0x00,qpos[0],qpos[1],qpos[2],qpos[3],0x00};

    int params[2] = {deviceId, val};
    Command abscmd(params, Command::ABS);
    QByteArray qa1 = abscmd.data();
    //qDebug() << val;
    qDebug() << qa1.toHex();

    QByteArray qa;
    array2qa(qa, qPos, 10);
    //qa.append(qa1).append(qa2);
    //qDebug() << qa.toHex();
    //m_list->append(qa1.append(qa2).toHex());
    //m_list->append(qa2.toHex());
    cmd_list->append(qa);

    QStringList list;
    list << tr("绝对运动指令") << QString(tr("绝对运行距离至 %1mm")).arg(position);


    model->insertRow(row, QModelIndex());
    model->setData(model->index(row, 0), list.value(0));
    model->setData(model->index(row, 1), list.value(1));
    //model->setData(model->index(row, 2), 1, Qt::UserRole);
    //自定义QModelIndex

    CommandLine cline(list.value(0), list.value(1), qa); //指令行对象
    lines.append(cline); //加入指令序列

    row++;

    ui->tableView->setModel(model);
}

void Form::on_relaAddBtn_clicked()
{
    int pos = ui->relMoveDistance->value();
    position += pos;
    moves.append(pos);

    int val = position*param;
    int params[2] = {deviceId, val};
    Command relacmd(params, Command::RELA);
    QByteArray qa1 = relacmd.data();
    //qDebug() << val;
    qDebug() << qa1.toHex();

    quint8 qpos[4];
    convert(qpos, position * param, 4);

    //qDebug() << QString("plus %1 pos %2").arg(position*param).arg(position);

    quint8 qPos[10] = {id[0],id[1],ABS_MOVE_CMD,0x01,0x00,qpos[0],qpos[1],qpos[2],qpos[3],0x00};

    QByteArray qa;
    array2qa(qa, qPos, 10);
    //qa.append(qa1).append(qa2);
    //qDebug() << qa.toHex();
    //m_list->append(qa1.append(qa2).toHex());
    //m_list->append(qa2.toHex());
    cmd_list->append(qa);

    QStringList list;
    list << tr("相对运动指令") << QString(tr("相对运行距离 %1mm")).arg(pos);

    model->insertRow(row, QModelIndex());
    model->setData(model->index(row, 0), list.value(0));
    model->setData(model->index(row, 1), list.value(1));

    CommandLine cline(list.value(0), list.value(1), qa); //指令行对象
    lines.append(cline); //加入指令序列

    row++;

    ui->tableView->setModel(model);
}

void Form::on_setSpdBtn_clicked()
{
    moves.append(0);

    int lpd = ui->setRunSpd->value(); //线速度mm/s - 转速 - 每秒脉冲数
    int spd = beta * lpd;
    //qDebug() << QString(tr("参数 %1 档位 %2%")).arg(beta).arg(spd);

    int params[2] = {deviceId, spd};
    Command spdcmd(params, Command::SPD);
    qDebug() << spdcmd.data().toHex();
    quint8 qspd[4];
    convert(qspd, spd, 4);

    quint8 qSpd[10] = {id[0],id[1],SETMOVESPCMD,0x01,0x00,qspd[0],qspd[1],qspd[2],qspd[3],0x00};
    QByteArray qa;
    array2qa(qa, qSpd, 10);
    cmd_list->append(qa);

    QStringList list;
    list << tr("设置速度指令") << QString(tr("线速度设置为 %1 mm/s")).arg(lpd);

    model->insertRow(row, QModelIndex());
    model->setData(model->index(row, 0), list.value(0));
    model->setData(model->index(row, 1), list.value(1));

    CommandLine cline(list.value(0), list.value(1), qa); //指令行对象
    lines.append(cline); //加入指令序列

    row++;

    ui->tableView->setModel(model);
}



void Form::on_stepAct_clicked()
{
    //if(index == row)
    //    index = 0;

    if((index >= 0) && (index < row)) {
        emit sendData(cmd_list->at(index));
        ui->tableView->selectRow(index);
        index++;
    }

    if(index == jmp_from)
        index = jmp_to;

    //qDebug() << "row = " << row << " index = " << index;

}

void Form::spd_show(int lpd)
{
    int circle = config->configs().circleLen;
    //int lpd = ui->setRunSpd->value();
    double rps = (double)lpd / circle;
    ui->rpsLab->setText(QString(tr("转速：%1 rps(转每秒)")).arg(rps));
}

void Form::convert(quint8 *buf, int data, int size)
{
    for(int i=0; i<size; i++)
        buf[i] = data >> (8*i);
}

void Form::array2qa(QByteArray &data, quint8 *buf, int size)
{
    for(int i=0; i<size; i++)
        data[i] = buf[i];
}

void Form::on_stopAct_clicked()
{
    /*
    quint8 qStop[10] = {id[0],id[1],EMSTOP_CMD,0x01,0x00,0x00,0x00,0x00,0x00,0x00};
    QByteArray qa;
    array2qa(qa, qStop, 10);
    emit sendData(qa);
    */
    int params[2] = {deviceId, 0};
    Command stopcmd(params, Command::STOP);

    qDebug() << stopcmd.data().toHex();
    emit sendData(stopcmd.data());
}

void Form::on_forwardAct_clicked()
{
    /*
    int ret = (quint8)echo.at(2);
    if(ret != 0x0f) {
        QMessageBox::warning(this, tr("提示"), tr("请停止运行"));
        return;
    }
    qDebug() << tr("下载程序成功");
    */
    int len = cmd_list->count();
    quint8 qlen[4];
    convert(qlen, len, 4);

    quint8 qHead[10] = {id[0],id[1],CMDBATCHHEAD,0x01,0x00,qlen[0],qlen[1],qlen[2],qlen[3],0x00};
    QByteArray qa;
    array2qa(qa, qHead, 10);
    //cmd_list->prepend(qa);

    //QByteArray data;
    for(int i=0; i<len; i++)
        qa.append(cmd_list->at(i));

    //qDebug() << qa.toHex();
    emit sendData(qa);

}


void Form::on_opAddBtn_clicked()
{
    moves.append(0);
    int param = ui->opParam->value();
    int opType = ui->opType->currentIndex() + 1; //1-自增 2-自减

    int params[3] = {deviceId, opType, param};
    Command opcmd(params, Command::OPER);
    qDebug() << opcmd.data().toHex();

    quint8 pop[4];
    convert(pop,opType,4);
    quint8 op[] = {id[0],id[1],OPERATEPARAM,param,0x00,pop[0],pop[1],pop[2],pop[3],0x00};
    QByteArray qa;
    array2qa(qa, op, 10);
    cmd_list->append(qa);

    QStringList list;
    if(opType == 1)
        list << tr("操作参数指令") << QString(tr("编号 %1 参数 ++")).arg(param);
    else if(opType == 2)
        list << tr("操作参数指令") << QString(tr("编号 %1 参数 --")).arg(param);

    model->insertRow(row, QModelIndex());
    model->setData(model->index(row, 0), list.value(0));
    model->setData(model->index(row, 1), list.value(1));

    CommandLine cline(list.value(0), list.value(1), qa); //指令行对象
    lines.append(cline); //加入指令序列

    row++;

    ui->tableView->setModel(model);

}

void Form::on_jmpAddBtn_clicked()
{
    moves.append(0);
    int line = ui->jmpLine->value();

    if(line > row) {
        QMessageBox::warning(this, tr("警告"), QString(tr("跳转行不能超过 %1 行")).arg(row));
        return;
    }

    int params[4] = {deviceId, 0, line, 0};
    Command jmpcmd(params, Command::JMP);
    qDebug() << jmpcmd.data().toHex();

    jmp_to = line-1;

    quint8 pln[2];
    convert(pln,line-1,2);
    quint8 ln[] = {id[0],id[1],JMP_CMD,0x00,0x00,0x00,0x00,pln[0],pln[1],0x00};
    QByteArray qa;
    array2qa(qa, ln, 10);
    cmd_list->append(qa);

    QStringList list;
    list << tr("无条件跳转指令") << QString(tr("无条件跳转至 %1 行指令")).arg(line);

    model->insertRow(row, QModelIndex());
    model->setData(model->index(row, 0), list.value(0));
    model->setData(model->index(row, 1), list.value(1));

    CommandLine cline(list.value(0), list.value(1), qa); //指令行对象
    lines.append(cline); //加入指令序列

    row++;
    jmp_from = row;

    ui->tableView->setModel(model);
}

void Form::on_cmpAddBtn_clicked()
{
    moves.append(0);
    int line = ui->cmpLine->value();
    if(line > row) {
        QMessageBox::warning(this, tr("警告"), QString(tr("跳转行不能超过 %1 行")).arg(row));
        return;
    }

    int param = ui->cmpParam->value();
    int type = ui->cmpType->currentIndex() + 1;
    int value = ui->cmpVal->value();

    jmp_to = line-1;

    int params[5] = {deviceId, value, line, param, type};
    Command cmpcmd(params, Command::CMP);
    qDebug() << cmpcmd.data().toHex();

    quint8 pln[2];
    convert(pln,line-1,2);
    quint8 val[2];
    convert(val, value, 2);
    quint8 ln[10] = {id[0],id[1],CMP_CMD,param,type,val[0],val[1],pln[0],pln[1],0x00};
    QByteArray qa;
    array2qa(qa, ln, 10);
    cmd_list->append(qa);

    QStringList list;
    list << tr("有条件跳转指令");
    if(type == 1)
        list << QString(tr("编号 %1 参数大于 %2 跳转至 %3 行指令")).arg(param).arg(value).arg(line);
    if(type == 2)
        list << QString(tr("编号 %1 参数等于 %2 跳转至 %3 行指令")).arg(param).arg(value).arg(line);
    if(type == 3)
        list << QString(tr("编号 %1 参数小于 %2 跳转至 %3 行指令")).arg(param).arg(value).arg(line);

    model->insertRow(row, QModelIndex());
    model->setData(model->index(row, 0), list.value(0));
    model->setData(model->index(row, 1), list.value(1));

    CommandLine cline(list.value(0), list.value(1), qa); //指令行对象
    lines.append(cline); //加入指令序列

    row++;
    jmp_from = row;

    ui->tableView->setModel(model);
}

void Form::on_jumpAddBtn_clicked()
{
    moves.append(0);
    int line = ui->jumpLine->value();
    if(line > row) {
        QMessageBox::warning(this, tr("警告"), QString(tr("跳转行不能超过 %1 行")).arg(row));
        return;
    }

    int param = ui->jumpParam->value();
    int state = ui->ioState->currentIndex();//0-低电平 1-高电平

    jmp_to = line-1;

    int params[4] = {deviceId, state, line, param};
    Command cmpcmd(params, Command::IOJMP);
    qDebug() << cmpcmd.data().toHex();

    quint8 pln[2];
    convert(pln,line-1,2);
    quint8 ln[] = {id[0],id[1],IOJUMP_CMD,param,0x00,state,0x00,pln[0],pln[1],0x00};
    QByteArray qa;
    array2qa(qa, ln, 10);
    cmd_list->append(qa);

    QStringList list;
    list << tr("IO条件跳转指令");
    if(!state)
        list << QString(tr("编号 %1 IO端口高电平跳转至 %2 行指令")).arg(param).arg(line);
    else
        list << QString(tr("编号 %1 IO端口低电平跳转至 %2 行指令")).arg(param).arg(line);

    model->insertRow(row, QModelIndex());
    model->setData(model->index(row, 0), list.value(0));
    model->setData(model->index(row, 1), list.value(1));

    CommandLine cline(list.value(0), list.value(1), qa); //指令行对象
    lines.append(cline); //加入指令序列

    row++;
    jmp_from = row;

    ui->tableView->setModel(model);
}

void Form::on_inputAddBtn_clicked() //输入等待
{
    moves.append(0);
    int param = ui->inputParam->value();
    int state = ui->inputState->currentIndex();

    int params[4] = {deviceId, state, 0, param};
    Command inputcmd(params, Command::INPUT);
    qDebug() << inputcmd.data().toHex();

    quint8 cmd[] = {id[0],id[1],INPUT_CMD,param,0x00,state,0x00,0x00,0x00,0x00};
    QByteArray qa;
    array2qa(qa, cmd, 10);
    cmd_list->append(qa);

    QStringList list;
    list << tr("输入等待指令");
    if(!state)
        list << QString(tr("编号 %1 输入端口低电平等待")).arg(param);
    else
        list << QString(tr("编号 %1 输入端口高电平等待")).arg(param);

    model->insertRow(row, QModelIndex());
    model->setData(model->index(row, 0), list.value(0));
    model->setData(model->index(row, 1), list.value(1));

    CommandLine cline(list.value(0), list.value(1), qa); //指令行对象
    lines.append(cline); //加入指令序列

    row++;

    ui->tableView->setModel(model);
}

void Form::on_outputAddBtn_clicked() //输出主动
{
    moves.append(0);
    int param = ui->outputParam->value();
    int state = ui->outputState->currentIndex();

    int params[4] = {deviceId, state, 0, param};
    Command outputcmd(params, Command::SETOUT);
    qDebug() << outputcmd.data().toHex();

    quint8 cmd[] = {id[0],id[1],SETOUT_CMD,param,0x00,state,0x00,0x00,0x00,0x00};
    QByteArray qa;
    array2qa(qa, cmd, 10);
    cmd_list->append(qa);

    QStringList list;
    list << tr("输出设置指令");
    if(!state)
        list << QString(tr("设置编号 %1 输出端口断开")).arg(param);
    else
        list << QString(tr("设置编号 %1 输出端口接通")).arg(param);

    model->insertRow(row, QModelIndex());
    model->setData(model->index(row, 0), list.value(0));
    model->setData(model->index(row, 1), list.value(1));

    CommandLine cline(list.value(0), list.value(1), qa); //指令行对象
    lines.append(cline); //加入指令序列

    row++;

    ui->tableView->setModel(model);
}

void Form::on_delayAddBtn_clicked()
{
    //引起指令模型变化
    // 1 增加一条指令序列
    // 2 单指令数据转化
    // 3 指令序列追加
    // 4 模型数据追加
    // 5 指令序列持久化

    // 抽象为单条指令与指令集合两个对象
    // 单条指令数据结构
    // 指令类型 数据格式化(通讯) 指令描述(展示)

    // 指令集合数据结构
    // 指令数据序列(通讯) 模型(展示) 状态(指令数量 当前执行)
    moves.append(0);
    int value = ui->delayVal->value();

    int params[2] = {deviceId, value};
    Command delaycmd(params, Command::DELAY);
    qDebug() << delaycmd.data().toHex();

    quint8 val[4];
    convert(val, value, 4);
    quint8 cmd[] = {id[0],id[1],DELAY_CMD,0x00,0x00,val[0],val[1],val[2],val[3],0x00};
    QByteArray qa;
    array2qa(qa, cmd, 10);
    cmd_list->append(qa);

    QStringList list;
    list << tr("延时等待指令") << QString(tr("延时等待 %1 毫秒")).arg(value);

    model->insertRow(row, QModelIndex());
    model->setData(model->index(row, 0), list.value(0));
    model->setData(model->index(row, 1), list.value(1));

    CommandLine cline(list.value(0), list.value(1), qa); //指令行对象
    lines.append(cline); //加入指令序列

    row++;

    ui->tableView->setModel(model);
}

void Form::tableDoubleClick(const QModelIndex &/*index*/)
{
    // 具体到某一行数据被双击
    // 需自定义一种模型，便于识别末行指令的类型
    // 然后展开相应的指令编辑区，用户修改指令
    // 数据模型还需要根据修改参数更新显示文本和指令
    //int nr = index.row();
    //qDebug() << tr("%1 row double clicked. data %2").arg(nr).arg(index.data().toString());

    //确定行 确定指令类型
    // 加入指令按钮有两种状态 新增和修改
    // 首次是新增 本方法改变状态为修改
    // 行确定修改模型
    // 类型确定修改指定模型行的数据
    // 指令和指令行对象均需要更新
}

void Form::tableClick(const QModelIndex &index)
{
    int nr = index.row();
    select_line = nr;
    qDebug() << tr("%1 row clicked. data %2").arg(nr+1).arg(index.data().toString());
}


void Form::on_clearBtn_clicked()
{
    cmd_list->clear();
    moves.clear();

    model->removeRows(0, row, QModelIndex());
    ui->tableView->setModel(model);
    row = 0;
    index = 0;
    position = 0;
}

void Form::on_deleteBtn_clicked()
{
    if(select_line < 0) {
        QMessageBox::information(this, tr("提示"), tr("请选择指令行！"));
        return;
    }
    /*
    row--;
    index--;
    if (row == 0) {
        //row = 0;
        position = 0;
        index = 0;
    }

    if(row == jmp_from)
        jmp_to = 0;

    model->removeRow(row, QModelIndex());
    cmd_list->removeAt(row);
    position -= moves.at(row);
    moves.removeAt(row);
    lines.removeAt(row);
    */
    row--;
    if(row < 0) row = 0;

    model->removeRow(select_line, QModelIndex());
    cmd_list->removeAt(select_line);
    position -= moves.at(select_line);
    moves.removeAt(select_line);
    lines.removeAt(select_line);
    qDebug() << tr("%1 行已被删除！").arg(select_line+1);

    select_line = -1;
}

void Form::on_editBtn_clicked()
{
    if(select_line < 0) {
        QMessageBox::information(this, tr("提示"), tr("请选择指令行！"));
        return;
    }
    //do your thing
    //有些事既然答应了就一鼓作气给搞定，否则一拖再拖，时间成本就就浪费了
    //抽个大空，把坑填完
    //修改指令需要知道当前选中行是什么类型指令


    select_line = -1; // back default value
}

void Form::on_insertBtn_clicked()
{

    if(select_line < 0) {
        QMessageBox::information(this, tr("提示"), tr("请选择指令行！"));
        return;
    } else {
        //插入当前行之前，之后
    }

}

void Form::on_upBtn_clicked()
{

    if(select_line < 0) {
        QMessageBox::information(this, tr("提示"), tr("请选择指令行！"));
        return;
    }
    if(select_line == 0) {
        QMessageBox::information(this, tr("提示"), tr("开始指令行无法上移！"));
        return;
    }
    //上移的操作
    model->moveRow(QModelIndex(),select_line,QModelIndex(),select_line-1);

    select_line = -1;
}

void Form::on_downBtn_clicked()
{
    if(select_line < 0) {
        QMessageBox::information(this, tr("提示"), tr("请选择指令行！"));
        return;
    }
    if(select_line == row-1) {
        QMessageBox::information(this, tr("提示"), tr("结束指令行无法下移！"));
        return;
    }
    //下移的操作
    model->moveRow(QModelIndex(),select_line,QModelIndex(),select_line+1);

    select_line = -1;
}
