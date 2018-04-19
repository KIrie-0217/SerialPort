#ifdef __linux

#include <dirent.h>
#include <sys/types.h>
#include <linux/serial.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <libgen.h>
#include <linux/limits.h>
#include <termios.h>

#include <fstream>
#include <unordered_map>
#include <algorithm>

#include "Serial.hpp"


using namespace std;

// /dev/tty***


// -------------------------------------------------------------------
// SerialInfo
const string no_device = "no device";
const string unknown = "unknown_device";

using PID = std::unordered_map<std::string, std::string >;
using database_map = std::unordered_map<std::string, std::pair<std::string, PID>>;

std::vector<std::string> split(std::string str, const std::string& sep) {
	std::vector<std::string> v;
	while (str.size() > 0) {
		unsigned long place = str.rfind(sep);
		if (place != std::string::npos) {
			if (place + 1 == str.size()) {
				str.pop_back();
				continue;
			}
			v.push_back(&str.c_str()[place + sep.size()]);
			while (str.size() != place) {
				str.pop_back();
			}
		}
		else {
			v.push_back(str.c_str());
			break;
		}
	}
	std::reverse(v.begin(), v.end());
	return v;
}

class DataBase {
private:
	database_map data;
public:
	DataBase() {
		ifstream ifs("/lib/udev/hwdb.d/20-usb-vendor-model.hwdb");
		if (!ifs)
			return;
		char buff[1024];
		vector<string> splited;
		string pid;
		string vid;
		string vendor;


		while (!ifs.getline(buff, 1024).eof()) {
			switch (buff[0]) {
			case ' ':
				splited = split(&buff[1], "=");
				// プロダクト
				if (splited[0] == "ID_MODEL_FROM_DATABASE") {
					data[vid].second[pid] = splited[1];
				}
				// ベンダー
				else if (splited[0] == "ID_VENDOR_FROM_DATABASE") {
					data[vid].first = splited[1];
				}

				break;
			case '\n':
			case '#':
			case '\0':
				continue;
				break;
			default:
				splited = split(buff, ":");
				if (splited[0] == "usb") {
					splited = split(splited[1], "p");
					// vendor
					if (splited.size() == 1) {
						splited[0].pop_back();
						vid = &splited[0].c_str()[1];
					}
					//product
					else {
						vid = &splited[0].c_str()[1];
						splited[1].pop_back();
						pid = splited[1].c_str();
					}
				}
				break;
			}
		}
		ifs.close();
	}
	string getName(const string& vid, const string& pid) {
		string vendor = data[vid].first;
		string product = data[vid].second[pid];
		if (vendor.size() > 0)
			return vendor + "  " + product;
		else
			return unknown;
	}
} usb_database;


const string SerialInfo::port() const {
	return port_name;
}
const string SerialInfo::device_name() const {
	return device;
}

SerialInfo::SerialInfo() {
	port_name = no_device;
	device = no_device;
}

SerialInfo::SerialInfo(const SerialInfo& info) :
	port_name(info.port_name),
	device(info.device) {
}

SerialInfo::SerialInfo(const string& _port) :
	port_name(_port) {
	char buffer[PATH_MAX];
	string dev_name = basename((char*)_port.c_str());
	string command = "find /sys/devices/ -name " + dev_name;
	FILE* fp = popen(command.c_str(), "r");
	if (!fp) {
		device = no_device;
		return;
	}
	fscanf(fp, "%s", buffer);
	pclose(fp);

	string path = buffer;
	if (path.find("usb") != string::npos) {
		string base;
		string vid;
		string pid;
		do {
			base = basename((char*)path.c_str());
			path.erase(path.end() - base.size() - 1, path.end());
		} while (base.find(":") == string::npos);

		ifstream ifs(path + "/idVendor");
		if (!ifs) {
			device = unknown;
			return;
		}
		ifs.getline(buffer, 1024);
		ifs.close();
		vid = buffer;

		ifs.open(path + "/idProduct");
		if (!ifs) {
			device = unknown;
			return;
		}
		ifs.getline(buffer, 1024);
		ifs.close();
		pid = buffer;
		device = usb_database.getName(vid, pid);
	}
	else {
		//port_name = no_device;
		device = unknown;
	}
}

SerialInfo::SerialInfo(const string & _port, const string & _device_name) :
	port_name(_port),
	device(_device_name) {
}

//----------------------------------------------------------------------
// getSerialList

const string get_driver(const string& dir) {
	struct stat st;
	// パスに/deviceを追加
	string devicedir = dir + "/device";

	// ファイル情報取得
	// シンボリックリンクかどうか
	if (lstat(devicedir.c_str(), &st) == 0 && S_ISLNK(st.st_mode)) {
		char buffer[1024];
		// readlinkがヌル文字を挿入しないため0で初期化
		for (int i = 0; i < 1024; i++) {
			buffer[i] = 0;
		}
		// パスに/driverを追加
		devicedir += "/driver";

		// ドライバがあれば返す
		if (readlink(devicedir.c_str(), buffer, sizeof(buffer)) > 0)
			return basename(buffer);
	}
	// ドライバ無し
	return "";
}

void probe_serial8250(vector<string>& portList, vector<string> portList8250) {
	struct serial_struct serialInfo;

	for (auto com8250 : portList8250) {
		// とりあえずopen
		int fd = open(com8250.c_str(), O_RDWR | O_NONBLOCK | O_NOCTTY);

		if (fd >= 0) {
			// 情報取得
			if (ioctl(fd, TIOCGSERIAL, &serialInfo) == 0) {
				// typeがPORT_UNKNOWNじゃなければコンソールじゃない
				if (serialInfo.type != PORT_UNKNOWN)
					portList.push_back(com8250);
			}
			close(fd);
		}
	}
}

vector<SerialInfo> getSerialList() {
	//ディレクトリ構造体
	struct dirent **nameList;
	//ポートリスト
	vector<string> portList;
	vector<string> portList8250;


	const string sysdir = "/sys/class/tty/";
	// ttyデバイスを列挙
	int listsize = scandir(sysdir.c_str(), &nameList, NULL, NULL);
	// エラー
	if (listsize < 0)
		return vector<SerialInfo>();
	else {
		for (int n = 0; n < listsize; n++) {
			// ../と./を省く
			if (std::string("..").compare(nameList[n]->d_name) && std::string(".").compare(nameList[n]->d_name)) {

				// フルパス(絶対パス)にする
				string devicedir = sysdir;
				devicedir += nameList[n]->d_name;

				// デバイスが使っているドライバの取得
				string driver = get_driver(devicedir);

				// ドライバがないならスキップ
				if (driver.size() > 0) {
					// /dev/tty***のパスに変換
					string devfile = "/dev/" + string(basename((char*)devicedir.c_str()));

					// serial8250はシリアルコンソール
					// シリアルコンソールとその他を分ける
					if (driver == "serial8250")
						portList8250.push_back(devfile);
					else
						portList.push_back(devfile);
				}
			}
			//　dirent構造体の開放
			free(nameList[n]);
		}
		//　dirent*の配列の開放
		free(nameList);
	}

	// シリアルコンソールの8250と本当の8250が混在しているので分ける
	probe_serial8250(portList, portList8250);

	vector<SerialInfo> list;
	for (auto port:portList) {
		list.push_back(SerialInfo(port));
	}
	return list;
}

// --------------------------------------------------------------------------
// Serial

const Serial::Config defconf = {
	9600,
	8,
	Serial::Config::Parity::NO,
	Serial::Config::StopBits::ONE,
};

void Serial::setBuffSize(unsigned long read, unsigned long write){
}

Serial::Serial(){
	conf = defconf;
	opened = false;
	handle = nullptr;
}

Serial::~Serial() {
	close();
}

// 重複防止
int lopen(const char* _file, int _oflag) { return open(_file, _oflag); }
int lclose(int fd) { return close(fd); }
int lread(int fd, void* buff, unsigned long size) { return (int)read(fd, buff, size); }
int lwrite(int fd, void* buff, unsigned long size) { return (int)write(fd, buff, size); }

struct descriptor_old_termios {
	struct termios tms;
	int fd;
};

bool Serial::open(const std::string & port_name, unsigned int baudRate){
	return open(SerialInfo(port_name), baudRate);
}

bool Serial::open(const SerialInfo & serial_info, unsigned int baudRate){
	info = serial_info;
	conf = defconf;
	conf.baudRate = baudRate;
	handle = new descriptor_old_termios;
	int& fd = ((descriptor_old_termios*)handle)->fd;
	struct termios tms;
	fd = lopen(info.port().c_str(), O_RDWR | O_NOCTTY);
	if (fd < 0) {
		delete ((descriptor_old_termios*)handle);
		return false;
	}
	// old
	tcgetattr(fd, &((descriptor_old_termios*)handle)->tms);
	tms = ((descriptor_old_termios*)handle)->tms;
	tms.c_iflag = 0;
	tms.c_oflag = 0;
	tms.c_lflag = 0;
	tms.c_cc[VMIN] = 1;
	tms.c_cc[VTIME] = 0;


	cfmakeraw(&tms);

	// ボーレート
	cfsetispeed(&tms, baudRate);
	cfsetospeed(&tms, baudRate);

	// パリティ
	switch(conf.parity){
	case Config::Parity::NO:
		tms.c_cflag &= ~PARENB;
		tms.c_cflag &= ~PARODD;
		break;
	case Config::Parity::EVEN:
		tms.c_cflag |= PARENB;
		tms.c_cflag &= ~PARODD;
		break;
	case Config::Parity::ODD:
		tms.c_cflag |= PARENB;
		tms.c_cflag |= PARODD;
		break;	
	}

	// データビット
	tms.c_cflag &= ~CSIZE;
	switch (conf.byteSize) {
	case 5:
		tms.c_cflag |= CS5;
		break;
	case 6:
		tms.c_cflag |= CS6;
		break;
	case 7:
		tms.c_cflag |= CS7;
		break;
	default:
		tms.c_cflag |= CS8;
		break;
	}

	// ストップビット
	switch (conf.stopBits) {
	case Config::StopBits::TWO:
		tms.c_cflag |= CSTOPB;
		break;
	default:
		tms.c_cflag &= ~CSTOPB;
		break;
	}
	
	// new
	//tcsetattr(fd, TCSANOW, &tms);
	tcsetattr(fd, TCSANOW, &tms);
	clear();
	opened = true;
	return true;
}

void Serial::close(){
	descriptor_old_termios* pointer = ((descriptor_old_termios*)handle);
	tcsetattr(pointer->fd, TCSANOW, &(pointer->tms));
	lclose(pointer->fd);
	delete ((descriptor_old_termios*)handle);
	opened = false;
}

const Serial::Config & Serial::getConfig() const {
	return conf;
}

void Serial::setConfig(const Config& _conf){
	conf = _conf;
	if (!opened)
		return;
	int& fd = ((descriptor_old_termios*)handle)->fd;
	struct termios tms;
	tcgetattr(fd, &tms);

	// パリティ
	switch (conf.parity) {
	case Config::Parity::NO:
		tms.c_cflag &= ~PARENB;
		tms.c_cflag &= ~PARODD;
		break;
	case Config::Parity::EVEN:
		tms.c_cflag |= PARENB;
		tms.c_cflag &= ~PARODD;
		break;
	case Config::Parity::ODD:
		tms.c_cflag |= PARENB;
		tms.c_cflag |= PARODD;
		break;
	}

	// データビット
	tms.c_cflag &= ~CSIZE;
	switch (conf.byteSize) {
	case 5:
		tms.c_cflag |= CS5;
		break;
	case 6:
		tms.c_cflag |= CS6;
		break;
	case 7:
		tms.c_cflag |= CS7;
		break;
	default:
		tms.c_cflag |= CS8;
		break;
	}

	// ストップビット
	switch (conf.stopBits) {
	case Config::StopBits::TWO:
		tms.c_cflag |= CSTOPB;
		break;
	default:
		tms.c_cflag &= ~CSTOPB;
		break;
	}

	// ボーレート
	cfsetispeed(&tms, conf.baudRate);
	cfsetospeed(&tms, conf.baudRate);
	// new
	tcsetattr(fd, TCSANOW, &tms);
}

const SerialInfo & Serial::getInfo() const {
	return info;
}

bool Serial::isOpened() const {
	return opened;
}

int Serial::read(unsigned char* data, int size){
	int error = lread(((descriptor_old_termios*)handle)->fd, data, size);
	return error;
}

unsigned char Serial::read1byte(){
	unsigned char data;
	//while (lread(((descriptor_old_termios*)handle)->fd, &data, 1) <= 0);
		//usleep(0.1);
	lread(((descriptor_old_termios *)handle)->fd, &data, 1);
	return data;
}

std::vector<unsigned char> Serial::read(){
	std::vector<unsigned char> data;
	unsigned char* buffer = new unsigned char[1024];
	int size;
	size = read(buffer, 1024);
	if (size <= 0) {
		delete[] buffer;
		//data.push_back(read1byte());
		return data;
	}
	data.reserve(size);
	for (int i = 0; i < size; i++) {
		data.push_back(buffer[i]);
	}
	delete[] buffer;
	return data;
}

void Serial::clear(){
	int& fd = ((descriptor_old_termios*)handle)->fd;
	tcflush(fd, TCIOFLUSH);
}

void Serial::clearWrite(){
	int& fd = ((descriptor_old_termios*)handle)->fd;
	tcflush(fd, TCOFLUSH);
}

void Serial::clearRead(){
	int& fd = ((descriptor_old_termios*)handle)->fd;
	tcflush(fd, TCIFLUSH);
}

int Serial::write(unsigned char* data, int size){
	return lwrite(((descriptor_old_termios*)handle)->fd, data, size);
}

int Serial::write(const std::vector<unsigned char>& data){
	int size = (int)data.size();
	unsigned char* buffer = new unsigned char[data.size()];
	for (int i = 0; i < size; i++) {
		buffer[i] = data[i];
	}
	size = write(buffer, size);
	delete[] buffer;
	return size;
}


#endif

/*
thanks
https://stackoverflow.com/questions/2530096/how-to-find-all-serial-devices-ttys-ttyusb-on-linux-without-opening-them
*/

/*
usb PID&VID list
http://www.linux-usb.org/usb.ids
*/

