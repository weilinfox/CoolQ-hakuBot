#include <iostream>
#include <set>
#include <sstream>
#include <fstream>
#include <cctype>
#include <ctime>

#include <cqcppsdk/cqcppsdk.h>

#include <windows.h>

using namespace cq;
using namespace std;
using Message = cq::message::Message;
using MessageSegment = cq::message::MessageSegment;

class Cdate {
public:
    bool operator< (const Cdate& a) const {
        return month*30+date < a.month*30+a.date;
    }
    Cdate():year(0), date(0), month(0) {}
    int year, date, month;
};

class CmainData {
public:
    static void poweroff();
    static string get(Cdate date);
    static void send(const Cdate& date);
    static void clearMsgMap(void);
    static bool repeatMsg(int64_t id, const string& msg);
    static bool quit;
    static bool run;
private:
    static void readFile (string groupId);
    static string getBirthday(const Cdate& date);
    static map <Cdate, set<string>> dataMap;
    static map <int64_t, string> msgMap;
    static map <int64_t, bool> msgBoolMap;
    static set <string> groupSet;
};

bool CmainData::run = true;
//bool CmainData::run = false;
bool CmainData::quit = false;
map<Cdate, set<string>> CmainData::dataMap;
map<int64_t, string> CmainData::msgMap;
map<int64_t, bool> CmainData::msgBoolMap;
//设置qq群
set<string> CmainData::groupSet = {"702031505", "1146440669"};

bool CmainData::repeatMsg(int64_t id, const string& msg)
{
    if (msgMap.count(id) && msgMap[id] == msg) {
        if (msgBoolMap[id]) {
            msgBoolMap[id] = false;
            return true;
        }
    } else {
        msgMap[id] = msg;
        msgBoolMap[id] = true;
    }
    return false;
}

void CmainData::clearMsgMap(void)
{
    msgMap.clear();
    msgBoolMap.clear();
    logging::debug("缓存", "清除static缓存");
}

string CmainData::get(Cdate date)
{
    char dt[20];
    sprintf(dt, "%04d-%02d-%02d :", date.year, date.month, date.date);
    string sendM(dt);
    for (auto it = groupSet.begin(); it != groupSet.end(); it++) {
        string ans = "";
        readFile(*it);
        ans = getBirthday(date);
        if (ans.length() > 0) {
            sendM += "\ngroup " + *it + " :\n" + ans;
        } else {
            sendM += "\ngroup " + *it + " :\nEmpty~";
        }
    }

    date.date++;
    int mon[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (date.year % 400 == 0) mon[2]++;
    else if (date.year % 100 == 0) ;
    else if (date.year % 4 == 0) mon[2]++;
    if (date.date > mon[date.month]) {
        date.date = 1;
        date.month++;
    }
    if (date.month > 12) {
        date.month = 1;
        date.year ++;
    }

    sprintf(dt, "\n%04d-%02d-%02d :", date.year, date.month, date.date);
    sendM += string(dt);
    for (auto it = groupSet.begin(); it != groupSet.end(); it++) {
        string ans = "";
        readFile(*it);
        ans = getBirthday(date);
        if (ans.length() > 0) {
            sendM += "\ngroup " + *it + " :\n" + ans;
        } else {
            sendM += "\ngroup " + *it + " :\nEmpty~";
        }
    }
    dataMap.clear();
    return sendM;
}

void CmainData::send(const Cdate& date)
{
    for (auto it = groupSet.begin(); it != groupSet.end(); it++) {
        string ans = "";
        readFile(*it);
        ans = getBirthday(date);
        if (ans.length() > 0) {
            int64_t id = 0;
            for (int i = 0; i < (*it).length(); i++)
                id *= 10, id += (*it)[i] - '0';
            try {
                send_group_message(id, ans);
            } catch (ApiError &) {
            }
        }
    }
    dataMap.clear();
}

string CmainData::getBirthday(const Cdate& date)
{
    string ans = "";
    if (dataMap.count(date)) {
        auto it = dataMap[date].begin();
        ans += *it;
        while ((++it) != dataMap[date].end())
            ans += '\n' + *it;
    }
    return ans;
}

void CmainData::readFile (string groupId)
{
    ifstream ifile;
    dataMap.clear();
    ifile.open(groupId + ".txt");
    if (ifile.fail())
        logging::info("读取", "没有文件："+groupId+".txt");
    else {
        int lines = 0;
        string str;
        //Cdate datet;
        while (getline(ifile, str)) {
            Cdate datet;
            stringstream ss(str);
            ss >> datet.month >> datet.date;
            ss >> str;
            dataMap[datet].insert(str);
            lines++;
        }
        str = "";
        if (lines == 0)
            str = "0";
        while (lines) {
            str = char(lines%10+'0') + str;
            lines /= 10;
        }
        ifile.close();
        logging::info("读取",  "文件："+groupId+".txt中读取 "+str+" 行");
    }
}

//关闭驻守进程
void CmainData::poweroff ()
{
    run = false;
}

void func (int id)
{
    switch (id) {
        case 0: {
            vector<Group> grouplist = get_group_list();
            for (auto it = grouplist.begin(); it != grouplist.end(); it++)
                send_group_message(it->group_id, "时间不早了，小白觉得该洗洗睡了。");
            break;
        }
        case 1: {
            //send_group_message(590048561, "今天是llh小哥哥的生日呢~");
            struct tm *lct;
            time_t timen;
            time(&timen);
            timen += 8*60*60;
            lct = gmtime(&timen);
            Cdate d;
            d.date = lct->tm_mday;
            d.month = lct->tm_mon + 1;
            d.year = lct->tm_year + 1900;
            CmainData::send(d);
            break;
        }
        default: {
            //send_group_message(590048561, "小白的test~");
            logging::info("test", "static test");
            break;
        }
    }
}

CQ_INIT {
    on_enable([] {
        logging::info("启用", "static启用后台");
        struct tm *lct;
        time_t timen;
        int sent = 0;
        int t_clear = 0;
        while (CmainData::run) {
            time(&timen);
            timen += 8*60*60; //utc+8
            lct = gmtime(&timen);
            //lct->tm_isdst = 0; //Daylight Saving Time flag
            if (lct->tm_hour == 23 && lct->tm_min == 30) {
                if (!sent) func(0), sent = 1; 
            } else if (lct->tm_hour == 0 && lct->tm_min == 0) {
                if (!sent) func(1), sent = 1;
            } /*else if (lct->tm_min == 35  && lct->tm_mday == 24 && lct->tm_mon+1 == 7) {
                if (!sent) func(100), sent = 1;
            } else if (lct->tm_min % 2 == 0) { //test
                if (!sent) logging::info("test", "static % 5"), sent = 1;
            }*/ else
                sent = 0;
            Sleep(10000);
            t_clear++;
            if (t_clear >= 30) {
                t_clear = 0;
                CmainData::clearMsgMap();
            }
            if (!CmainData::run) {
                CmainData::quit = true;
                logging::info("关闭", "static关闭后台");
                CmainData::clearMsgMap();
            }
        } 
    });

    on_group_message([](const GroupMessageEvent &event) {
        try {
            if (CmainData::run) {
                if (CmainData::repeatMsg(event.group_id, event.message))
                    send_group_message(event.group_id, event.message);
            }
        } catch (ApiError&) {
        }
    });

    on_private_message([](const PrivateMessageEvent &event) {
        try {
            if (event.message == ".status") {
                if (!CmainData::quit && CmainData::run) {
                    send_private_message(event.user_id, "小白的static一直在关注着~");
                    logging::info("查询", "static状态正常");
                } else {
                    send_private_message(event.user_id, "小白的static已经关掉了~");
                    logging::info("查询", "static已关闭");
                }
            } else if (event.message == ".poweroff") {
                if (event.user_id == 2521857263 || event.user_id == 2750510860) {
                    CmainData::poweroff();
                    send_private_message(event.user_id, "小白只听主人的~\nstatic在关了");
                }
            } else if (event.message == ".test") {
                if (event.user_id == 2521857263 || event.user_id == 2750510860) {
                    struct tm *lct;
                    time_t timen;
                    time(&timen);
                    timen += 8*60*60;
                    lct = gmtime(&timen);
                    Cdate d;
                    d.date = lct->tm_mday;
                    d.month = lct->tm_mon + 1;
                    d.year = lct->tm_year + 1900;
                    string str = CmainData::get(d);
                    send_private_message(event.user_id, str);
                }
            }
        } catch (ApiError &) {
        }
    });
}

