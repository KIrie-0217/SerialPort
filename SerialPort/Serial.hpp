#pragma once
#include <string>
#include <vector>
#include <initializer_list>

class SerialPort {
public:
	//�ݒ�
	struct State {
		unsigned int baudRate;
		unsigned int byteSize;
		enum class Parity {
			NO,//�p���e�B�Ȃ�
			EVEN,//�����p���e�B
			ODD//��p���e�B
		} parity;
		enum class StopBits {
			//1�r�b�g
			ONE,
			//1.5�r�b�g
			ONE5,
			//2�r�b�g
			TWO
		} stopBits;
		bool useParity;
	};
private:
	//�|�[�g�ԍ�
	const unsigned int num;
	//�|�[�g��
	const std::string name;
	//�t���l�[��
	const std::string fullname;
	bool isOpened;

	State state;

	void* handle;

	SerialPort() = delete;
	SerialPort(const unsigned int num, const std::string name, const std::string fullname);
public:
	SerialPort(const SerialPort&);
	~SerialPort();
	//�f�t�H���g�ݒ�
	bool open();
	//�ݒ�
	bool open(const State&);
	void close();
	State& getState();
	void setState();

	std::vector<char> receive();
	void send(std::initializer_list<char>);

	const unsigned int getNumber() const;
	const std::string& getPortName() const;
	const std::string& getFullName() const;
	static std::vector<SerialPort> getList();
};