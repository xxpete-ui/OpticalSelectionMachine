#ifndef MODBUSTCPSLAVE_H
#define MODBUSTCPSLAVE_H
#include <string>
#include <mutex>
#include "modbus.h"

class ModbusTcpSlave
{
private:
    ModbusTcpSlave();
    ~ModbusTcpSlave();

public:
    void Open(int _port = 502);	//打开
    void Close();				//关闭

    static ModbusTcpSlave* GetPtr();	//获取单例
    void DeletePtr();			//释放单例指针
private:
    void State_Read();	//状态读取（写入modbus离散输入）
    void State_Write();	//状态写入（从线圈读取）
    void Thread_Keep();	//定时任务读写状态
    void Accept(int sk);//连接主机及请求处理
    void Receive();
    void updateStatusJson();   // 将品类数据序列化json写入输入寄存器

    bool read_coil_status(int addr, int nb, uint8_t* dest);			//读线圈状态 01
    bool write_multiple_coil(int addr, int nb, const uint8_t* data);//写多个线圈 15
    bool read_registers(int addr, int nb, uint16_t* dest);			//读取多个保持寄存器
    bool write_registers(int addr, int nb, uint16_t* dest);			//写多个保持寄存器 16

private:
    modbus_t* m_bus = nullptr;			//modbus对象
    modbus_mapping_t* m_map = nullptr;	//modbus虚拟内存结构
    int port = 502;						//端口

    static ModbusTcpSlave* m_slave;		//单例
    static std::mutex mtx;				//锁
};

#endif // MODBUSTCPSLAVE_H
