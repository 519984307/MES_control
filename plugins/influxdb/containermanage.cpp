﻿#include "containermanage.h"
#include <QTime>
#include <QThread>
ContainerManage::ContainerManage(QWidget *parent)
{
    loadDataBase();        //加载数据库配置
    loadDataTable();       //加载数据表配置
    m_time=new QTimer();
    connect(m_time,SIGNAL(timeout()),this,SLOT(timeOut()));
    connect(this,SIGNAL(saveDataToDB(int)),this,SLOT(autoSave(int)));
    m_time->start(100);
}


ContainerManage::~ContainerManage()
{
    m_time->stop();
    QSqlDatabase db;
    for(int index=0;index<MaxDataBase;index++  ){
        db=QSqlDatabase::database(dataBaseInfor[index].name);
        if(db.isValid()){
        db.close();
        QSqlDatabase::removeDatabase(dataBaseInfor[index].name);
       }
    }
}
/*从主程序框架接收消息*/
void ContainerManage::receiveMsgFromManager(ResponseMetaData response)
{
    /*判断消息是否发送给对话框界面的*/
    if(response.target=="dialog"){
      ResponseMetaData_dialog data;
      if(response.type==getDrivesInfor){
         data.type="getDrivesInfor";
      }
      if(response.type==getDataSetInfor){
         data.type="getDataSetInfor";
      }
      if(response.type==setValue){
         data.type="setValue";
      }
      if(response.type==getValue){
         data.type="getValue";
      }
      data.value=response.value;
      emit sendMsgToDialog(data);
      return;
    }
    switch(response.type){
    case getDrivesInfor:
        driveInfor=response.value;
        break;
    case getDataSetInfor:
        dateSetInfor=response.value;
        break;
    case getValue:
        dataTableInfor[response.target.toInt()].getValueResult=response.value;
        break;
    case setValue:
        dataTableInfor[response.target.toInt()].getValueResult=response.value;
        break;
    }
}
/*处理从对话框接收的信号*/
void ContainerManage::receiveMsgFromDialog(RequestMetaData_dialog request)
{
        RequestMetaData data;
        data.from="dialog";
        data.target="pluginManage";
        data.drive=request.drive;
        data.index=request.index;
        data.value=request.value;
        if(request.type=="getDrivesInfor"){
           data.type=getDrivesInfor;
        }
        if(request.type=="getDataSetInfor"){
           data.type=getDataSetInfor;
        }
        if(request.type=="setValue"){
           data.type=setValue;
        }
        if(request.type=="getValue"){
           data.type=getValue;
        }
        emit sendMsgToManager(data); //转发信息到插件管理器
}

void ContainerManage::showForm(QWidget *parent)
{
  ConfigDialog *m_config=new ConfigDialog(parent);
  connect(m_config,SIGNAL(SendMsgToContainerManage(RequestMetaData_dialog)),this,SLOT(receiveMsgFromDialog(RequestMetaData_dialog)));
  connect(this,SIGNAL(sendMsgToDialog(ResponseMetaData_dialog)),m_config,SLOT(receiveMsgFromContainerManage(ResponseMetaData_dialog)));
  m_config->exec();
  loadDataBase();
  loadDataTable();
}

void ContainerManage::autoSave(int id)
{
    int index=id;
    int dataBase=dataTableInfor[index].dataBase.toInt();
    QSqlDatabase db=QSqlDatabase::database(dataBaseInfor[dataBase].name);
    if(!db.isValid()){
        db.close();
        QSqlDatabase::removeDatabase(dataBaseInfor[dataBase].name);
        db= QSqlDatabase::addDatabase("QMYSQL",dataBaseInfor[dataBase].name);
        db.setHostName(dataBaseInfor[dataBase].address);
        db.setPort(dataBaseInfor[dataBase].port.toInt());
        db.setDatabaseName(dataBaseInfor[dataBase].name);
        db.setUserName(dataBaseInfor[dataBase].username);
        db.setPassword(dataBaseInfor[dataBase].password);
        if(!db.open())
        {
           db.close();
           QSqlDatabase::removeDatabase(dataBaseInfor[dataBase].name);
           return;
        }
    }
     QJsonDocument document = QJsonDocument::fromJson(dataTableInfor[index].rules.toUtf8());
     QJsonArray array= document.array();
     QString  fields="";
     QString  values="";
     for (int i = 1; i < array.count(); i++)
      {
         QJsonObject value = array.at(i).toObject();
         if(value["dataIndex"].toString()!=""){
             RequestMetaData data;
             data.type=getValue;
             data.from=QString::number(index);
             data.target="manage";
             data.drive=value["drive"].toString();
             data.index=value["dataIndex"].toString();
             emit sendMsgToManager(data);
             while(dataTableInfor[index].getValueResult==""){
             }
            fields.append(value["field"].toString());
            fields.append(",");
            values.append("'");
            values.append(dataTableInfor[index].getValueResult);
            values.append("'");
            values.append(",");
            dataTableInfor[index].getValueResult="";
          }
      }
     fields.remove(fields.length()-1,1);
     values.remove(values.length()-1,1);
     QString cmd="insert into "+dataTableInfor[index].name+"("+fields+") " +"values ("+values+");";
     QSqlQuery  query(db);
     query.exec(cmd);
     count++;
     qDebug()<<count;
}
/*时间定时器超时(槽)*/
void ContainerManage::timeOut()
{
  time_count++;
  for(int i=0;i<MaxDataTable;i++)
  {
    if(dataBaseInfor[dataTableInfor[i].dataBase.toInt()].enable==true){
        if(dataTableInfor[i].enable==true && dataTableInfor[i].name!=""){
            qDebug()<<100*time_count<<dataTableInfor[i].frequency.toLongLong();
           if(100*time_count%(dataTableInfor[i].frequency.toLongLong())==0){
             emit saveDataToDB(i);
           }
        }
    }
  }
}


/*加载数据库信息*/
void ContainerManage::loadDataBase()
{
    QString fileName="../plugins/config/influxdbconfig.log";
    QFile file(fileName);
   if (!file.open(QFile::ReadOnly)) {   //如果文件不存在则新建文件
       file.open( QIODevice::ReadWrite | QIODevice::Text );
      }
   QString line ;
   QStringList list;
   QString   Data[9];
   for(int row = 0;row<MaxDataBase;row++){
      line = file.readLine(200);
      if (!line.startsWith('#') && line.contains(',')) {
         list = line.simplified().split(',');
         if(list.length()<9){
           for (int i = 0; i < list.length();i++){
              Data[i]=list.at(i);
            }
           dataBaseInfor[row].name=Data[0];
           dataBaseInfor[row].enable=Data[1].toInt();
           dataBaseInfor[row].desc=Data[2];
           dataBaseInfor[row].username=Data[3];
           dataBaseInfor[row].password=Data[4];
           dataBaseInfor[row].address=Data[5];
           dataBaseInfor[row].port=Data[6];
         }
       }
     }
    file.close();
}
/*加载数据表信息*/
void ContainerManage::loadDataTable()
{
    QString fileName="../plugins/config/influxdbconfig1.log";
    QFile file(fileName);
   if (!file.open(QFile::ReadOnly)) {   //如果文件不存在则新建文件
       file.open( QIODevice::ReadWrite | QIODevice::Text );
      }
   QString line ;
   QStringList list;
   QString   Data[9];
   for(int row = 0;row<MaxDataTable;row++){
      line = file.readLine(10000);
      if (!line.startsWith('#') && line.contains(';')) {
         list = line.simplified().split(';');
         if(list.length()<9){
           for (int i = 0; i < list.length();i++){
              Data[i]=list.at(i);
            }
           dataTableInfor[row].name=Data[0];
           dataTableInfor[row].dataBase=Data[1];
           dataTableInfor[row].enable=Data[2].toInt();
           dataTableInfor[row].frequency=Data[3];
           dataTableInfor[row].desc=Data[4];
           dataTableInfor[row].rules=Data[5];
         }
       }
     }
    file.close();
}
