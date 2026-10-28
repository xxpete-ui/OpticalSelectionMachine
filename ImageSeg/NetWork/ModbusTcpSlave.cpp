#include "ModbusTcpSlave.h"

#include <thread>
#include <vector>
#include <qDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QByteArray>

#include "Server/State.h"

// 定义存放 JSON 的起始寄存器地址和最大长度
#define JSON_REG_START   400
#define JSON_REG_MAX      400   // 最多 400 个 register = 800 字节


ModbusTcpSlave* ModbusTcpSlave::m_slave = nullptr;
std::mutex ModbusTcpSlave::mtx;

ModbusTcpSlave::ModbusTcpSlave() {}

ModbusTcpSlave::~ModbusTcpSlave() {}



void ModbusTcpSlave::Open(int _port)
{
    if (State::ModbusInfo::IsOpen)
        return;
    port = _port;
    std::string ip = State::ModbusInfo::IPAddress;
    m_bus = modbus_new_tcp(ip.c_str(), port);
    if (m_bus == nullptr)
    {
        m_bus = modbus_new_tcp(ip.c_str(), port);
        if (m_bus = nullptr)
        {
            //MsgControl::GetMsgPtr()->WriteControlMsg(L"Modbus从站初始化失败！");
            //Log::GetPtr()->Error("Modbus从站初始化失败，文件：ModbusTcp.cpp，行：17。modbus_new_tcp(ip.c_str(), port) 返回 nullptr");
            return;
        }
    }

    modbus_set_slave(m_bus, 1);		//设置从站地址

    m_map = modbus_mapping_new(100, 100, 800, 800);
    if (m_map == nullptr)
    {
        m_map = modbus_mapping_new(100, 100, 800, 800);
        if (m_map == nullptr || m_map == NULL)
        {
            modbus_free(m_bus);
            //MsgControl::GetMsgPtr()->WriteControlMsg(L"Modbus从站初始化失败！");
            //Log::GetPtr()->Error("Modbus从站初始化内存区域失败，文件：ModbusTcp.cpp，行：31。modbus_mapping_new(10, 20, 10, 120); 返回 nullptr");
            m_slave->Close();
            return;
        }
    }
    memset(m_map->tab_input_bits, 0, sizeof(m_map->tab_input_bits));
    memset(m_map->tab_bits, 0, sizeof(m_map->tab_bits));
    memset(m_map->tab_registers, 0, sizeof(m_map->tab_registers));
    memset(m_map->tab_input_registers, 0, sizeof(m_map->tab_input_registers));

    State::ModbusInfo::IsOpen = true;
    //MsgControl::GetMsgPtr()->WriteControlMsg(L"Modbus从站打开成功");
    //Log::GetPtr()->Info("Modbus从站初始化完成，打开成功，开始监听任务");

    int socket = modbus_tcp_listen(m_bus, 1);	//设置监听数量

    std::thread th_2(&ModbusTcpSlave::Thread_Keep, this);
    th_2.detach();

    std::thread th_accept(&ModbusTcpSlave::Accept, this, socket);
    th_accept.detach();
}


void ModbusTcpSlave::Accept(int sk)
{
    while (State::ModbusInfo::IsOpen)
    {
        int socket = sk;
        if (socket == -1)
        {
            //MsgControl::GetMsgPtr()->WriteControlMsg(L"电脑IP设置错误，Modbus从站连接失败！");
            //Log::GetPtr()->Error("电脑IP设置错误，Modbus从站连接失败！");
            m_slave->Close();
            break;
        }
        modbus_tcp_accept(m_bus, &socket);		//等待主机连接

        //Log::GetPtr()->Info("Modbus从站收到主站连接，开始处理主站请求任务");

        std::thread th_recv(&ModbusTcpSlave::Receive, this);
        th_recv.detach();

        //while (Servers::State::ModbusServer::IsOpen)
        //{
        //	// 接收来自主机的请求
        //	uint8_t query[MODBUS_TCP_MAX_ADU_LENGTH];
        //	int rc = modbus_receive(m_bus, query);

        //	if (rc > 0)
        //	{
        //		// 处理请求并回复主机
        //		modbus_reply(m_bus, query, rc, m_map);
        //	}
        //	else if (rc == -1)
        //	{
        //		// 连接中断
        //		MsgControl::GetMsgPtr()->WriteControlMsg(L"Modbus主站断开连接");
        //		Log::GetPtr()->Error("Modbus从站处理主站请求任务失败，连接中断！");
        //		break;
        //	}
        //}
    }
}

void ModbusTcpSlave::Receive()
{
    while (State::ModbusInfo::IsOpen)
    {
        // 接收来自主机的请求
        uint8_t query[MODBUS_TCP_MAX_ADU_LENGTH];
        int rc = modbus_receive(m_bus, query);

        if (rc > 0)
        {
            // 处理请求并回复主机
            modbus_reply(m_bus, query, rc, m_map);
        }
        else if (rc == -1)
        {
            // 连接中断
            //MsgControl::GetMsgPtr()->WriteControlMsg(L"Modbus主站断开连接");
            //Log::GetPtr()->Error("Modbus从站处理主站请求任务失败，连接中断！");
            break;
        }
    }
}




void ModbusTcpSlave::Close()
{
    State::ModbusInfo::IsOpen = false;
    if (m_bus != nullptr)
    {
        if (m_map != nullptr || m_map != NULL)
        {
            modbus_mapping_free(m_map);
            m_map = nullptr;
        }
        modbus_close(m_bus);
        modbus_free(m_bus);
        m_bus = nullptr;
    }
    //MsgControl::GetMsgPtr()->WriteControlMsg(L"Modbus从站已关闭");
    //Log::GetPtr()->Info("Modbus从站已关闭");
}


ModbusTcpSlave* ModbusTcpSlave::GetPtr()
{
    if (m_slave == nullptr)
    {
        std::unique_lock<std::mutex> lock(mtx);
        if (m_slave == nullptr)
            m_slave = new ModbusTcpSlave();
    }
    return m_slave;
}

void ModbusTcpSlave::DeletePtr()
{
    if (State::ModbusInfo::IsOpen)
        this->Close();

    if (m_slave != nullptr)
    {
        delete m_slave;
        m_slave = nullptr;
    }
}



bool ModbusTcpSlave::read_coil_status(int addr, int nb, uint8_t* dest)
{
    int ret = modbus_read_bits(m_bus, addr, nb, dest);   //成功时返回读取输入位的数目即nb，失败时返回-1.
    return ret == nb;
}

bool ModbusTcpSlave::write_multiple_coil(int addr, int nb, const uint8_t* data)
{
    int ret = modbus_write_bits(m_bus, addr, nb, data);		//成功时返回写入寄存器个数即nb，失败时返回-1.
    return ret == nb;
}

bool ModbusTcpSlave::read_registers(int addr, int nb, uint16_t* dest)
{
    int ret = modbus_read_registers(m_bus, addr, nb, dest);
    return ret == nb;
}

bool ModbusTcpSlave::write_registers(int addr, int nb, uint16_t* dest)
{
    int ret = modbus_write_registers(m_bus, addr, nb, dest);
    return ret == 1;
}


void ModbusTcpSlave::State_Read()
{
    if (State::ModbusInfo::IsOpen && m_map)
    {
        m_map->tab_input_bits[0] = true;
        m_map->tab_input_bits[1] = !State::imgProcessInitSuccess;
        m_map->tab_input_bits[2] = !State::imgProcessInitSuccess;
        m_map->tab_input_bits[3] = !State::imgProcessInitSuccess;
        m_map->tab_input_bits[4] = State::PLCInfo::IsOpen;
        m_map->tab_input_bits[5] = State::PLCInfo::SpeedState;
        m_map->tab_input_bits[6] = State::PLCInfo::LightSourceState;
        //m_map->tab_input_bits[7] = Servers::State::PLC::震动筛状态;
        m_map->tab_input_bits[7] = false;
        m_map->tab_input_bits[8] = State::PLCInfo::AutoRun;
        m_map->tab_input_bits[9] = State::PLCInfo::EmergencyStop;
        m_map->tab_input_bits[10] = State::PLCInfo::LackOfAirPressure;
        m_map->tab_input_bits[11] = State::PLCInfo::FrequencyConverterFailure;
        m_map->tab_input_bits[12] = State::PLCInfo::AirValvePositionError;



    }

}


void ModbusTcpSlave::State_Write()
{
    if (State::ModbusInfo::IsOpen && m_map)
    {
        /*uint8_t data[3];
        read_coil_status(0, 3, data);
        Servers::State::PLC::全设备急停 = (data[0] == 1 ? true : false);
        Servers::State::PLC::后方设备异常 = (data[1] == 1 ? true : false);
        Servers::State::PLC::控制柜暂停运行 = (data[2] == 1 ? true : false);*/

        State::PLCInfo::AllEmergencyStops = m_map->tab_bits[0];
        State::PLCInfo::RearEquipmentAbnormality = m_map->tab_bits[1];
        State::PLCInfo::ControlCabinetStop = m_map->tab_bits[2];
    }
}

void ModbusTcpSlave::Thread_Keep()
{
    while (State::ModbusInfo::IsOpen)
    {
        State_Read();
        State_Write();
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}


void ModbusTcpSlave::updateStatusJson()
{
    if (!m_map) return;
    QJsonObject json;   // 构建 JSON 对象

    std::vector<int> nums(State::typePoint.size(), 0);
    for(auto item = State::typeCounts.begin(); item != State::typeCounts.end(); item++)
    {
        auto point = State::typePoint.find(item->first);
        if(point != State::typePoint.end())
        {
            nums[point->second] += item->second;
        }
        json.insert(QString::fromStdString(item->first), item->second.load());
    }

    // 1.写入数量
    for (int i = 0; i < nums.size() * 2; i+=2)
    {
        uint32_t data = nums[i / 2];

        if (data == 0)
            continue;

        std::vector<uint16_t> u16(2);
        u16[0] = (uint16_t)(data >> 16);
        u16[1] = (uint16_t)(data & 0xffff);
        m_map->tab_input_registers[100 + i] = u16[0];
        m_map->tab_input_registers[101 + i] = u16[1];
    }


    ////测试读取数据
    // uint32_t u32[40] = { 0 };
    // for (int j = 0; j < nums.size() * 2; j += 2)
    // {
    //     u32[j / 2] = m_map->tab_input_registers[101 + j] | (m_map->tab_input_registers[100 + j] << 16);
    // }
    // for (int i = 0; i < 24; i++)
    // {
    //     if(u32[i] > 0)
    //         qDebug() << "index = " + std::to_string(i) + ", num = " + std::to_string(u32[i]);
    // }


    // 2.写入品类字典数据
    QJsonDocument doc(json);        // 序列化为 JSON 字符串（紧凑格式，节省空间）
    QByteArray utf8Data = doc.toJson(QJsonDocument::Compact); // 或使用 Indented 更易读但更长
    //限制长度，防止溢出
    int maxBytes = JSON_REG_MAX * 1;          // 每个寄存器存 1 字节，最多存 JSON_REG_MAX 字节
    if (utf8Data.size() > maxBytes) {
        utf8Data.truncate(maxBytes);
        qWarning() << "JSON data truncated to fit input registers";
    }

    //清零目标寄存器区域（可选）
    for (int i = 0; i < JSON_REG_MAX; ++i) {
        m_map->tab_input_registers[JSON_REG_START + i] = 0;
    }

    //将每个字节写入一个寄存器（低 8 位）
    for (int i = 0; i < utf8Data.size(); ++i) {
        m_map->tab_input_registers[JSON_REG_START + i] = static_cast<uint16_t>(utf8Data[i]) & 0x00FF;
    }

}
