/*
SR600控制流程
1.查看motor连接情况；2.读取motor编码器位置；3.判断偏离0位置情况；4.重新校0和极限位置；5.等待键盘输入目标位置
*/
#include <iostream>
#include "actuatorcontroller.h"
#include <thread>
#include <signal.h>
#include <atomic>
#include <string>
using namespace std;

atomic_bool bExit{false};
atomic_bool inputFlag{false};
atomic_int value_1{0};
atomic_int value_2{0};
atomic_int value_3{0};
atomic_int value_4{0};
vector<double> currentPositon;


/***************************************************
 * Fuction: read motor position value from terminal
 * Input: in terminal--value_1 value_2 value_3 value_4
****************************************************/
void keyboardInput()
{
    double a;
    double b;
    double c;
    double d;
    while (!bExit)
    {
        if(true)
        {
            cin >> a;
            cin >> b;
            cin >> c;
            cin >> d;
            if(!cin.fail())
            {
                value_1 = a*1000;
                value_2 = b*1000;
                value_3 = c*1000;
                value_4 = d*1000;

                inputFlag = true;
            }
        }
    }
}

/***************************************************
 * Fuction: read target motor position from keyboardInput()
 * Input: motor positon value
****************************************************/
void setTargetPosition(double value_1,double value_2,double value_3,double value_4)
{
    ActuatorController *pController = ActuatorController::getInstance();
    vector<uint8_t> idArray = pController->getActuatorIdArray();
    pController->activeActuatorMode(idArray,Actuator::Mode_Profile_Pos);    //active Profile Positiion Mode

    pController->setPosition(idArray.at(0), value_1);                       //set position value
    pController->setPosition(idArray.at(1), value_2);
    pController->setPosition(idArray.at(2), value_3);
    pController->setPosition(idArray.at(3), value_4);
}

/***************************************************
 * Fuction: read current motor position
****************************************************/
void readCurrentPostion()
{
    ActuatorController * pController = ActuatorController::getInstance();
    vector<uint8_t> idArray = pController->getActuatorIdArray();
    for(int i=0;i<idArray.size();i++)
    {
        currentPositon.push_back(pController->getPosition(idArray.at(i),true));
        //currentPositon[i]=pController->getPosition(idArray.at(i),true);
    }
}

/***************************************************
 * Fuction: Capture "ctl+c" in terminal to stop the program
****************************************************/
void processSignal(int sign)
{
    ActuatorController::getInstance()->closeAllActuators();
    this_thread::sleep_for(std::chrono::milliseconds(200));     //delay some time to ensure close actuators successfully
    bExit = true;
}


int main(int argc, char *argv[])
{
    ActuatorController::initController(Actuator::Via_Serialport);           //init the controller via serial port
    signal(SIGINT, processSignal);                                          //register a SIGINT signal
    thread inputThread(keyboardInput);                                      //declar a inputThread
    ActuatorController * pController = ActuatorController::getInstance();   //declar a controller object

    int nLaunchedActuatorCnt = 0;
    //connect controller operation signal
    //uint8_t nDevicedId is actuator ID,uint8_t operationType is operation type
    //operation type: Recgnize_Finished,Launch_Finished,Close_Finished,Save_Params_Finished,
    int nOperationConnection = pController->m_sOperationFinished->s_Connect([&](uint8_t nDeviceId,uint8_t operationType){
        switch (operationType) {
        case Actuator::Recognize_Finished:
            if(pController->hasAvailableActuator())
            {
                vector<uint8_t> idArray = pController->getActuatorIdArray();
                cout << "Number of connected actuators:" << idArray.size() << endl;
                for (uint8_t id: idArray)
                {
                    if(pController->getActuatorAttribute(id,Actuator::ACTUATOR_SWITCH)==Actuator::ACTUATOR_SWITCH_OFF)
                    {
                        pController->launchActuator(id);    //if there is closed actuator,launch it
                    }
                }
            }
            break;
        case Actuator::Launch_Finished:                     //each motor will send this signal once
            if(++nLaunchedActuatorCnt == pController->getActuatorIdArray().size())
            {
                cout << "All actuators have launched!"  << endl;

                readCurrentPostion();
                cout<<"Current positon: ";
//                vector<uint8_t> idArray = pController->getActuatorIdArray();
//                cout<<pController->getPosition(idArray.at(0),true)<<" ";
//                cout<<pController->getPosition(idArray.at(1),true)<<" ";
//                cout<<pController->getPosition(idArray.at(2),true)<<" ";
//                cout<<pController->getPosition(idArray.at(3),true)<<endl;
                for(int i=0;i<currentPositon.size();i++)
                {
                    cout<<currentPositon.at(i)<<" ";
                }
                cout<<endl;
            }
            break;
        default:
            break;
        }
    });

    //connect controller attribute signal
    //uint8_t nDevicedId is actuator ID,uint8_t nAttrId is actuator attribute ID,double value is the value of attribute
    int nAttrConnection =pController->m_sActuatorAttrChanged->s_Connect([=](uint8_t nDeviceId,uint8_t nAttrId,double value){
        //cout << "Actuator ID: " << (int)nDeviceId << endl;
        //cout << "atribute ID: " << (int)nAttrId << endl;
        //cout << "atribute value: " << value << endl;
        //cout << "----------------------------"<<endl;
    });


    //connect controller error signal
    //uint8_t nDeviceId is actoator ID,uint16_t nErrorType is error type,string errorInfo is information of error
    int nErrorConnection = pController->m_sError->s_Connect([=](uint8_t nDeviceId,uint16_t nErrorType,string errorInfo){
        if(nDeviceId==0)
            cout << "Error: " << (int)nErrorType << " " << errorInfo << endl;
        else
            cout << "Actuator " << (int)nDeviceId << " " <<"error " << (int)nErrorType << " " << errorInfo << endl;
    });

    //regognize the linking actuators automaticly
    pController->autoRecoginze();

    //controller process events loop--should not block the processEvent()
    while (!bExit)
    {
        ActuatorController::processEvents();
        if(inputFlag==true)
        {
            setTargetPosition(value_1/1000.0,value_2/1000.0,value_3/1000.0,value_4/1000.0);
            inputFlag = false;
            value_1 = 0;
            value_2 = 0;
            value_3 = 0;
            value_4 = 0;
        }
    }

    //disconnect controller signal
    pController->m_sOperationFinished->s_Disconnect(nOperationConnection);
    pController->m_sActuatorAttrChanged->s_Disconnect(nAttrConnection);
    pController->m_sError->s_Disconnect(nErrorConnection);

    inputThread.join();
    return 0;
}