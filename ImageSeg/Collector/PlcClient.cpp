#include "PlcClient.h"
#include <thread>
#include <chrono>
#include "Server/State.h"
#include "Server/Server.h"

// 定义静态成员变量（分配存储）
PlcClient* PlcClient::m_plc = nullptr;
std::mutex PlcClient::m_mtx;   // 如果你的单例锁是静态成员，也需要定义

PlcClient::PlcClient() {}


PlcClient::~PlcClient()
{

}

bool PlcClient::write_single_coil(int coil_addr, int status)
{
    int ret = modbus_write_bit(mb_t, coil_addr, status);  //成功时，返回1；失败时，返回-1。
    return ret >= 0;
}

bool PlcClient::write_multiple_coil(int addr, int nb, const uint8_t* data)
{
    int ret = modbus_write_bits(mb_t, addr, nb, data);		//成功时返回写入线圈个数即nb，失败时返回-1.
    return ret == nb;
}

bool PlcClient::read_coil_status(int addr, int nb, uint8_t* dest)
{
    int ret = modbus_read_bits(mb_t, addr, nb, dest);   //成功时返回读取输入位的数目即nb，失败时返回-1.
    return ret == nb;
}

bool PlcClient::write_register(int addr, uint16_t dest)
{
    int ret = modbus_write_register(mb_t, addr, dest);
    return ret == 1;
}

bool PlcClient::read_registers(int addr, int nb, uint16_t* dest)
{
    int ret = modbus_read_registers(mb_t, addr, nb, dest);
    return ret == nb;
}

void PlcClient::DeletePtr()
{
    if(State::PLCInfo::IsOpen)
        this->Close();

    if (m_plc != nullptr)
    {
        delete m_plc;
        m_plc = nullptr;
    }
}

PlcClient* PlcClient::GetPlcPtr()
{
    if (m_plc == nullptr)
    {
        std::unique_lock<std::mutex> lock(m_mtx);
        if (m_plc == nullptr)
            m_plc = new PlcClient();
    }
    return m_plc;
}

void PlcClient::Timer_1S()
{
    try
    {
        while (State::PLCInfo::IsOpen)
        {
            PlcClient::GetPlcPtr()->ReadState();
            PlcClient::GetPlcPtr()->WriteWarning();

            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
        //Log::GetPtr()->Error("PLC未连接或连接断开，定时读取设备状态失败");
    }
    catch (...)
    {
        //Log::GetPtr()->Error("循环读写设备状态异常");
    }

}



void PlcClient::Open()
{
    int Error = 0;

    while (!State::PLCInfo::IsOpen && Error < 5)
    {
        try
        {
            //MsgControl::GetMsgPtr()->WriteControlMsg(L"正在连接PLC");
            std::string ip = State::PLCInfo::IPAddress;
            int port = State::PLCInfo::Port;
            if (port == 0 || port == NULL)
                port = 502;
            mb_t = modbus_new_tcp(ip.c_str(), port);

            if (MODBUS_ERR == modbus_set_slave(mb_t, 1)
                || MODBUS_ERR == modbus_set_response_timeout(mb_t, 0, 1000 * 200)  //!< [100ms]，注意，此处的响应延时需要根据实际情况设定！！！
                || MODBUS_ERR == modbus_connect(mb_t))                             //
            {
                //连接失败
                modbus_close(mb_t);
                modbus_free(mb_t);
                //MsgControl::GetMsgPtr()->WriteControlMsg(L"PLC连接失败，正在尝试重连！");
                //Log::GetPtr()->Error("连接PLC失败，IP = " + ip + "，port = " + to_string(port) + "，尝试次数 ：" + to_string(Error));
                Error++;
                continue;
            }
            State::PLCInfo::IsOpen = true;
            //Servers::State::PLC::IsAlive = true;

            //MsgControl::GetMsgPtr()->WriteControlMsg(L"PLC连接成功！");
            //Log::GetPtr()->Info("PLC连接成功");

            SetConfig();
            ChangeHandState();

            std::thread th_1s(&PlcClient::Timer_1S, this);
            th_1s.detach();

            break;

        }
        catch (...)
        {
            //MsgControl::GetMsgPtr()->WriteControlMsg(L"PLC连接异常！");
            //Log::GetPtr()->Error("PLC连接异常");
        }
    }
    if (Error >= 5)
    {
        State::PLCInfo::IsOpen = false;
        //MsgControl::GetMsgPtr()->WriteControlMsg(L"PLC连接失败！");
        //Log::GetPtr()->Error("PLC连接失败！");
    }
}


void PlcClient::Close()
{
    State::PLCInfo::IsOpen = false;
    if (mb_t != nullptr)
    {
        modbus_close(mb_t);
        //modbus_free(mb_t);
        if (mb_t != nullptr)
        {
            delete mb_t;
            mb_t = nullptr;
        }
    }
    //MsgControl::GetMsgPtr()->WriteControlMsg(L"PLC连接已关闭！");
    //Log::GetPtr()->Info("PLC连接关闭成功");
}


bool PlcClient::SetConfig()
{
    if (!State::PLCInfo::IsOpen)
    {
        //MsgControl::GetMsgPtr()->WriteControlMsg(L"修改失败！PLC未连接！");
        //Log::GetPtr()->Error("错误操作：PLC未连接，修改输送速率失败！");
        return false;
    }
    bool ret = write_register(304, State::PLCInfo::Speed);
    write_register(305, State::BlowsSet::PoreCount);
    return ret;
}


bool PlcClient::ChangeHandState()
{
    if (!State::PLCInfo::IsOpen)
    {
        //MsgControl::GetMsgPtr()->WriteControlMsg(L"操作失败！PLC未连接！");
        //Log::GetPtr()->Error("错误操作：PLC未连接，手动操作失败！");
        return false;
    }


    bool ret = false;
    ret = write_single_coil(50, State::PLCInfo::ManualSpeedRun);
    ret = write_single_coil(51, State::PLCInfo::ManualLightSource);
    //ret = write_single_coil(52, Servers::State::PLC::震动入料手动);

    return ret;
}

bool PlcClient::Clean_valve()
{
    if (!State::PLCInfo::IsOpen)
    {
        //MsgControl::GetMsgPtr()->WriteControlMsg(L"操作失败！PLC未连接！");
        //Log::GetPtr()->Error("错误操作：PLC未连接，阀组缝隙清理失败！");
        return false;
    }
    write_single_coil(55, true);
    //Log::GetPtr()->Info("执行阀组缝隙清理成功");
    return true;
}

void PlcClient::ReadState()
{
    uint8_t data[9];
    read_coil_status(60, 9, data);
    State::PLCInfo::SpeedState = data[0];
    State::PLCInfo::LightSourceState = data[1];
    //State::PLCInfo::震动筛状态 = data[2];
    State::PLCInfo::AutoRun = data[3];
    State::PLCInfo::EmergencyStop = data[4];
    State::PLCInfo::LackOfAirPressure = data[5];
    State::PLCInfo::FrequencyConverterFailure = data[6];
    State::PLCInfo::AirValvePositionError = data[8];

    if(!State::OrderRun && State::PLCInfo::SpeedState && State::PLCInfo::LightSourceState)
    {
        State::OrderRun = true;
        Server::GetPtr()->getCollectorPtr()->startRun();
    }
    else if(State::OrderRun && (!State::PLCInfo::SpeedState) && (!State::PLCInfo::LightSourceState))
    {
        State::OrderRun = false;
    }
}

void PlcClient::WriteWarning()
{
    /*uint8_t byte = 1;
    write_single_coil(54, byte);*/

    uint8_t data[3];
    data[0] = State::PLCInfo::RearEquipmentAbnormality ? 1 : 0;
    data[1] = State::PLCInfo::AllEmergencyStops ? 1 : 0;
    data[2] = State::PLCInfo::ControlCabinetStop ? 1 : 0;

    if(State::imgProcessInitSuccess)
        write_single_coil(59, true);

    write_single_coil(54, true);
    write_multiple_coil(56, 3, data);
}
