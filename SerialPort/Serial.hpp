#pragma once
#include <string>
#include <vector>
#include <initializer_list>

struct SerialInfo {
	//�|�[�g�ԍ�
	unsigned int port;
	//�|�[�g��
	std::string name;
	//�t���l�[��
	std::string dev_name;
	SerialInfo();
	SerialInfo(const unsigned int, const std::string name, const std::string dev_name);
	SerialInfo(const unsigned int);
};

std::vector<SerialInfo> serialList();

class Serial {
public:
	//�ݒ�
	struct Config {
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
	//�|�[�g���
	SerialInfo info;

	//�I�[�v�����Ă邩
	bool opened;

	//�ݒ�
	Config conf;

	void* handle;
	void setBuffSize(size_t read, size_t write);

public:
	Serial();
	Serial(const Serial&) = delete;
	~Serial();

	//<sammary>
	//�f�o�C�X���I�[�v��
	//</sammary>
	bool open(unsigned int port, unsigned int baudRate = 9600);
	//<sammary>
	//�f�o�C�X���N���[�Y
	//</sammary>
	void close();

	//<sammary>
	//�|�[�g���̎擾
	//</sammary>
	const Config& getConfig() const;
	//<sammary>
	//�|�[�g����ݒ�
	//</sammary>
	void setConfig(const Config&);
	//<sammary>
	//�f�o�C�X���̎擾
	//</sammary>
	const SerialInfo& getInfo() const;
	//<sammary>
	//�f�o�C�X���I�[�v�����Ă��邩
	//</sammary>
	bool isOpened() const;

	//<sammary>
	//��M�o�b�t�@�̃o�C�g��
	//</sammary>
	size_t available() const;
	//<sammary>
	//��M(�񐄏�)
	//</sammary>
	bool read(unsigned char* data, size_t size);
	//<sammary>
	//1�o�C�g��M
	//</sammary>
	unsigned char read1byte();
	//<sammary>
	//�o�b�t�@���ׂĎ�M
	//</sammary>
	std::vector<unsigned char> read();


	//<sammary>
	//�o�b�t�@���N���A
	//</sammary>
	void clear();
	//<sammary>
	//�o�̓o�b�t�@���N���A
	//</sammary>
	void clearWrite();
	//<sammary>
	//���̓o�b�t�@���N���A
	//</sammary>
	void clearRead();

	//<sammary>
	//���M
	//</sammary>
	void write(unsigned char* data, size_t size);
	//<sammary>
	//���M
	//</sammary>
	void write(const std::vector<unsigned char>& data);
};