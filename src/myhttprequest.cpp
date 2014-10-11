#include "myhttprequest.h"
#include "utility.h"

MyHttpRequest::MyHttpRequest(QObject *parent) :
    QObject(parent)
{
    m_status = Idle;
    manager = new NetworkAccessManager(this);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    request.setRawHeader ("Referer", "http://d.web2.qq.com/proxy.html?v=20110331002&callback=1&id=2");
    connect (manager, SIGNAL(finished(QNetworkReply*)), SLOT(finished(QNetworkReply*)));
}

MyHttpRequest::RequestStatus MyHttpRequest::status()
{
    return m_status;
}

void MyHttpRequest::setStatus(MyHttpRequest::RequestStatus new_status)
{
    if( new_status!=m_status ) {
        m_status = new_status;
        emit statusChanged();
    }
}

NetworkAccessManager *MyHttpRequest::getNetworkAccessManager()
{
    return manager;
}

void MyHttpRequest::finished(QNetworkReply *reply)
{
    Data temp = queue_data.dequeue ();
    
    ReplyType type=temp.replyType;
    bool isError = !(reply->error () == QNetworkReply::NoError);
    
    if(type==CallbackFun){
        QJSValueList list;
        list.append (QJSValue(isError));
        list.append (QJSValue(isError?reply->errorString ():reply->readAll ()));//
        temp.callbackFun.call (list);
    }else if(type==ConnectSlot){
        QObject* obj = temp.caller;
        QByteArray slotName = temp.slotName;
        QVariant str;
        
        if(!QMetaObject::invokeMethod (obj, slotName.data (), Qt::AutoConnection, Q_RETURN_ARG(QVariant, str), Q_ARG(QVariant, isError), Q_ARG(QVariant, (isError?reply->errorString ():reply->readAll ()))))//
            qDebug()<<"调用槽"+slotName+"失败";
    }
    //delete temp;//记得销毁数据
    send();//继续请求
}

void MyHttpRequest::send(QJSValue callbackFun, QUrl url, QByteArray data, bool highRequest/*=false*/)
{
    if((!callbackFun.isCallable ())||url.toString ()=="")
        return;
    if(highRequest){
        qDebug()<<"高优先级的网络请求";
        new MyHttpRequestPrivate(callbackFun, url, data);//进行高优先级的网络请求
    }else {
        requestData request_data;
        request_data.url = url;
        request_data.data = data;
        queue_requestData<<request_data;
        
        Data temp;
        temp.callbackFun = callbackFun;
        temp.replyType = CallbackFun;
        queue_data<<temp;
        if(status ()==Idle){
            send();
        }
    }
}

void MyHttpRequest::get(QObject *caller, QByteArray slotName, QUrl url, bool highRequest/*=false*/)
{
    send(caller, slotName, url, "", highRequest);
}

void MyHttpRequest::post(QObject *caller, QByteArray slotName, QUrl url, QByteArray data, bool highRequest/*=false*/)
{
    send(caller, slotName, url, data, highRequest);
}

void MyHttpRequest::get(QJSValue callbackFun, QUrl url, bool highRequest/*=false*/)
{
    send (callbackFun, url, "", highRequest);
}

void MyHttpRequest::post(QJSValue callbackFun, QUrl url, QByteArray data, bool highRequest/*=false*/)
{
    send (callbackFun, url, data, highRequest);
}

void MyHttpRequest::send(QObject *caller, QByteArray slotName, QUrl url, QByteArray data, bool highRequest/*=false*/)
{
    //qDebug()<<QThread::currentThread ();
    if(caller==NULL||slotName==""||url.toString ()=="")
        return;
    if(highRequest){
        qDebug()<<"高优先级的网络请求";
        new MyHttpRequestPrivate(caller, slotName, url, data);//进行高优先级的网络请求
    }else{
        requestData request_data;
        request_data.url = url;
        request_data.data = data;
        queue_requestData<<request_data;
        
        Data temp;
        temp.caller = caller;
        temp.slotName = slotName;
        temp.replyType = ConnectSlot;
        queue_data<<temp;
        //qDebug()<<queue_data.count ()<<queue_requestData.count ()<<QThread::currentThread ();
        if(status ()==Idle){
            send();
            //QTimer::singleShot (10, this, SLOT(send()));//不然可能会堵塞ui线程
        }
    }
}

void MyHttpRequest::abort()
{
    if(!m_reply.isNull ()){
        m_reply->abort ();
    }
}

void MyHttpRequest::send()
{
    //qDebug()<<queue_data.count ()<<queue_requestData.count ()<<QThread::currentThread ();
    if( queue_requestData.count ()>0){
        setStatus (Busy);
        requestData temp = queue_requestData.dequeue ();
        request.setUrl (temp.url);
        QByteArray data = temp.data;
        if( data=="" )
            m_reply = manager->get (request);
        else
            m_reply = manager->post (request, data);
    }else
        setStatus (Idle);//设置状态为空闲
}

QString MyHttpRequest::errorString()
{
    return m_reply->errorString ();
}

void MyHttpRequest::setRawHeader(const QByteArray &headerName, const QByteArray &value)
{
    request.setRawHeader (headerName, value);
}


MyHttpRequestPrivate::MyHttpRequestPrivate(QJSValue callbackFun, QUrl url, QByteArray data):
    MyHttpRequest(0)
{
    qDebug()<<"创建了高优先级的网络请求";
    send(callbackFun, url, data, false);
}

MyHttpRequestPrivate::MyHttpRequestPrivate(QObject *caller, QByteArray slotName, QUrl url, QByteArray data):
    MyHttpRequest(0)
{
    qDebug()<<"创建了高优先级的网络请求";
    send(caller, slotName, url, data, false);
}

void MyHttpRequestPrivate::finished(QNetworkReply *reply)
{
    MyHttpRequest::finished (reply);
    deleteLater ();//销毁自己
}
