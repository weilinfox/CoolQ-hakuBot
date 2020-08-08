#include <iostream>
#include <set>
#include <sstream>
#include <cctype>
#include <ctime>

#include <cqcppsdk/cqcppsdk.h>

using namespace cq;
using namespace std;
using Message = cq::message::Message;
using MessageSegment = cq::message::MessageSegment;

string help ()
{
    return \
"小白现在懂得下面几个命令呦:\n\
.bc [args]\n\
.echo [args]\n\
.help\n\
.ping [args]\n\
.time [args]\n\
有啥不明白的试试 命令+help";
}

string lctime(const string& args)
{
    int tz = 8;
    if (args == "help")
        return "小白还是晓得中国标准时间滴~\n或者告诉小白想要的时区";
    else if (args.length() > 0) {
        int flag = 1;
        if (!isdigit(args[0]) && args[0] != '-')
            flag = 0;
        for (int i = 1; flag && i < args.length(); i++)
            if (!isdigit(args[i])) {
                flag = 0;
                break;
            }
        if (flag) {
            int tmp = 0;
            int i = 0;
            if (args[0] == '-')
                i = 1;
            for (; i < args.length(); i++) {
                tmp *= 10;
                tmp += args[i] - '0';
            }
            if (args[0] == '-')
                tmp = -tmp;
            if (tmp <= 12 && tmp >= -12)
                tz = tmp;
            else
                return "不要骗小白，没有这个时区，哼~";
        }
    }
    struct tm *ltm;
    time_t rawtime;
    time(&rawtime);
    rawtime += tz*60*60;
    ltm = gmtime(&rawtime);
    //ltm->tm_isdst = 1;
    if (tz == 8) return string(asctime(ltm)) + "UTC+8";
    else if (tz < 0) return string(asctime(ltm)) + "UTC" + args;
    else if (tz == 0) return string(asctime(ltm)) + "UTC";
    else return string(asctime(ltm)) + "UTC+" + args;
}

string ping ()
{
    return "pong!";
}

string bc (string args)
{
    if (args == "help" || args.length() == 0)
        return "狸说用C++复刻个bc太烦了，就教了小白A类不确定度……";
    for (int i = 0; i < args.length(); i++)
        if (!isdigit(args[i]) && args[i] != ' ' && args[i] != '.')
            return "好像有不是数的东西……";
    stringstream ss(args);
    vector<double> vct;
    double tmp;
    double sum = 0.0, ua = 0.0;
    while (ss >> tmp) {
        vct.push_back(tmp);
        sum += tmp;
    }
    if (vct.size() == 0)
        return "好像没有正常的数来着……";
    sum /= vct.size();
    for (auto it = vct.begin(); it != vct.end(); it++)
        ua += pow((*it) - sum, 2);
    ua /= vct.size()-1;
    ua = sqrt(ua);

    char ava[20], unc[20];
    sprintf(ava, "%.6lf", sum);
    sprintf(unc, "%.6lf", ua);

    return "平均值为: " + string(ava) + "\nA类不确定度为: " + string(unc);
}

string echo (const string& args)
{
    if (args.length() == 0)
        return "感觉心里空空的……";
    int rnd = (int)clock() % 10;
    switch (rnd)
    {
    case 0: 
        return "小白是狐不是狗！";
    case 1:
        return "可是小白觉得犬夜叉是坠吼的~";
    case 2: case 7:
        return "介个这样念的嘛~\n" + args;
    case 3: case 5: case 6: case 9:
        return args;
    case 4: case 8:
        return "汝不会念嘛:\n" + args;
    default:
        return "什么东西出错了……一定是狸不是小白！";
    }
}

string selectFunc (const string& com)
{
    stringstream ss(com);
    string args, fun, tmp;
    //char ch;

    ss >> fun;
    
    getline(ss, args);
    while (getline(ss, tmp)) {
        args += '\n' + tmp;
    }
    while (args.length() > 0 && args[0] == ' ')
        args.erase(0, 1);
    int tails = args.length();
    int len = tails;
    while (args[--tails] == ' ');
    if (tails != len - 1)
        args.erase(tails+1, len - tails + 1);
    
    if (fun == "小白")
        return "小白在呢~";
    else if (fun == ".ping")
        return ping();
    else if (fun == ".bc")
        return bc(args);
    else if (fun == ".echo")
        return echo(args);
    else if (fun == ".help")
        return help();
    else if (fun == ".time")
        return lctime(args);
    /*else if (fun[0] == '.' && fun.length() > 1)
        return "小白不懂" + fun.substr(1, fun.length()-1) + "是什么……";*/
    else
        return "";
}

CQ_INIT {
    on_enable([] { logging::info("启用", "func已启用"); });

    on_private_message([](const PrivateMessageEvent &event) {
        try {
            string reply = selectFunc(event.message);
            if (reply.length() > 0) {
                send_private_message(event.user_id, reply);
                event.block();
            }
        } catch (ApiError &) {
        }
    });

    on_message([](const MessageEvent &event) {
        logging::debug("消息", "收到消息: " + event.message + "\n实际类型: " + typeid(event).name());
        try {
            if (event.message.length() > 6) {
                int flag = 0;
                string str = event.message;
                for (int i = 0; !flag && i <= str.length() - 6; i++) {
                    if (str.substr(i, 6) == "小白")
                        flag = 1;
                }
                if (flag) {
                    //logging::debug("消息", "收到小白");
                    Message msg("");
                    msg += MessageSegment::face(175);
                    msg.send(event.target);
                }
            }
        } catch (ApiError&) {
        }
    });

    on_group_message([](const GroupMessageEvent &event) {
        //static const set<int64_t> ENABLED_GROUPS = {123456, 123457};
        //if (ENABLED_GROUPS.count(event.group_id) == 0) return; // 不在启用的群中, 忽略

        try {
            string reply = selectFunc(event.message);
            if (reply.length() > 0) {
                send_group_message(event.group_id, reply);
                event.block();
            }
            //logging::info_success("组内ping", "pong!, 消息 Id: " + to_string(msgid));
            //auto msgid = send_message(event.target, "pong!"); // 复读
            /*auto mem_list = get_group_member_list(event.group_id); // 获取群成员列表
            string msg;
            for (auto i = 0; i < min(10, static_cast<int>(mem_list.size())); i++) {
                msg += "昵称: " + mem_list[i].nickname + "\n"; // 拼接前十个成员的昵称
            }
            send_group_message(event.group_id, msg); // 发送群消息*/
        } catch (ApiError &) { // 忽略发送失败
        }

        /*if (event.is_anonymous()) {
            logging::info("群聊", "消息是匿名消息, 匿名昵称: " + event.anonymous.name);
        }*/
    });
    
    on_group_member_increase([](const auto &event) {
        try {
            Message msg("");
            msg += MessageSegment::at(event.user_id);
            msg += " 欢迎欢迎，我是小白~进群了就是一家人了~\n可以 .help 瞅瞅我能干啥哦~";
            msg.send(event.target);
        } catch (ApiError&) {
        }
    });

    /*on_group_upload([](const auto &event) { // 可以使用 auto 自动推断类型
        stringstream ss;
        ss << "您上传了一个文件, 文件名: " << event.file.name << ", 大小(字节): " << event.file.size;
        try {
            send_message(event.target, ss.str());
        } catch (ApiError &) {
        }
    });*/
}

CQ_MENU(menu_demo_1) {
    logging::info("func菜单", "发送ping");
    try {
        send_private_message(2521857263, ping());
        send_private_message(2750510860, ping());
    } catch (ApiError &) { // 忽略发送失败
    }
}
