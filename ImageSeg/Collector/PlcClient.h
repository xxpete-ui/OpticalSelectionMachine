#ifndef PLCCLIENT_H
#define PLCCLIENT_H

#include <mutex>
#include <modbus.h>


class PlcClient
{
private:
    PlcClient();
    ~PlcClient();

    bool write_single_coil(int coil_addr, int status);				//写单个线圈
    bool write_multiple_coil(int addr, int nb, const uint8_t* data);//写多个线圈
    bool read_coil_status(int addr, int nb, uint8_t* dest);			//读线圈状态
    bool write_register(int addr, uint16_t dest);					//写单个保持寄存器
    bool read_registers(int addr, int nb, uint16_t* dest);			//读取多个保持寄存器
    void ReadState();		//读取设备状态
    void WriteWarning();	//写入设备状态
    void Timer_1S();
public:
    void Open();			//连接plc
    void Close();			//关闭连接
    bool SetConfig();		//设置光选机输送速率
    bool ChangeHandState();	//改变手动操作状态
    bool Clean_valve();		//光选机阀组缝隙清理

    void DeletePtr();		//释放单例指针
    static PlcClient* GetPlcPtr();	//获取单例

private:
    modbus_t* mb_t;				//通过modbus tcp

    static PlcClient* m_plc;		//单例指针
    static std::mutex m_mtx;	//单例锁


    static constexpr int MODBUS_ERR = -1;	// libmodbus's error return value
};

#endif // PLCCLIENT_H
