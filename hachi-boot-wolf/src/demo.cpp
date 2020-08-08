#include <iostream>
#include <set>
#include <sstream>
#include <fstream>
#include <cctype>
#include <ctime>

#include <cqcppsdk/cqcppsdk.h>

#include <windows.h>

#define PLAYERMAX 7
#define PLAYERMIN 5
#define WOLFDEBUG

using namespace cq;
using namespace std;
using Message = cq::message::Message;
using MessageSegment = cq::message::MessageSegment;

void gettime (struct tm* tms)
{
    time_t timen;
    time(&timen);
    timen += 8*60*60; //utc+8
    tms = gmtime(&timen);
}

enum players {witch, prophet, hunter, wolf1, wolf2, civilian1, civilian2};

class groupData {
public:
    groupData(int64_t gid);
    int setNumOfPlayers (int n);// 设置玩家总数，成功返回人数，否则返回0
    int addPlayer (int64_t);    // 添加玩家，成功返回缺几人，否则返回-1
    void startGame (void);      // 开始游戏
    int getStep (void);         // 返回游戏当前步骤
    void nextStep (bool);       // 游戏进行下一步
    void sendIdentity (void);   // 私发玩家身份
    void sendPlayersPos (void); // 群中发送玩家编号 (playersId pos+1)
    void getMessage(bool, int64_t, int);
                                // 是否收到有效回复 (true group false private)
private:
    int64_t groupId;            // 群ID
    int numOfPlayers;           // 玩家数量
    set <int64_t> playersIdSet; // 添加玩家
    int64_t playersId[10];      // 生成玩家列表
    string playersName[10];     // 生成玩家昵称（群名片）
    int identityPos[10];        // 游戏身份对应的玩家列表下标
    int identityAlive[10];      // 每个游戏身份的生死
    time_t lastActiveTime;      // 该游戏房间最后活跃时间
    bool gameAvailable;         // 游戏是否可以开始
    int gameStep;               // 游戏进行到的布置
    int killId[3];              // 0狼人1女巫2猎人 击杀对象下标 -1为无
    int release;                // 女巫是否解救
    bool antidote;              // 解药
    bool poison;                // 毒药
    void generateIdentity(void);// 游戏初始化
};

groupData::groupData(int64_t gid)
    :numOfPlayers(0),
    gameAvailable(false),
    groupId(gid),
    gameStep(0)
{
    time(&lastActiveTime);
}

int groupData::setNumOfPlayers(int n)
{
    playersIdSet.clear();
    gameAvailable = false;
    time(&lastActiveTime);
    if (n > PLAYERMAX || n < PLAYERMIN) return 0;
    else return numOfPlayers = n;
}

int groupData::addPlayer (int64_t id)
{
    time(&lastActiveTime);
    if (playersIdSet.size() < numOfPlayers)
        playersIdSet.insert(id);
    else
        return -1; // full

    if (numOfPlayers == playersIdSet.size()) {
        gameAvailable = true;
        //gameStep = 0;
        antidote = poison = true;
        release = 0;
        memset(killId, -1, sizeof(killId));
        generateIdentity();
    }
    return numOfPlayers - playersIdSet.size();
}

void groupData::generateIdentity(void)
{
    int n = 0, t;
    int *pos;
    for (auto it = playersIdSet.begin(); it != playersIdSet.end(); it++) {
        playersId[n++] = *it;
    }
    pos = new int[numOfPlayers];
    memset(pos, 0, sizeof(pos));
    while (n) {
        t = rand() % numOfPlayers;
        if (!pos[t]) {
            pos[t] = 1;
            identityPos[--n] = t;
        }
    }
    delete[] pos;
    pos = NULL;

    vector<GroupMember> mblist = get_group_member_list(groupId);
    map<int64_t, string> mbMap;
    for (auto it = mblist.begin(); it != mblist.end(); it++) {
        mbMap[it->user_id] = it->card;
    }
    for (int i = 0; i < numOfPlayers; i++) {
        playersName[i] = mbMap[playersId[i]];
    }

#ifdef WOLFDEBUG
    string str = "";
    for (int i = 0; i < numOfPlayers; i++)
        str += ' ' + char(identityPos[i] + '0');
    logging::debug("wolf", "identiry:" + str);
#endif

    return;
}

void groupData::sendIdentity(void)
{
    try {
        send_private_message(playersId[identityPos[witch]], "女巫");
        send_private_message(playersId[identityPos[prophet]], "预言家");
        send_private_message(playersId[identityPos[hunter]], "猎人");
        send_private_message(playersId[identityPos[wolf1]], "狼人，你的同伴编号为 " + char(identityPos[wolf2] + '0'));
        send_private_message(playersId[identityPos[wolf2]], "狼人，你的同伴编号为 " + char(identityPos[wolf1] + '0'));
        if (numOfPlayers > 5)
        send_private_message(playersId[identityPos[civilian1]], "平民");
        if (numOfPlayers > 6)
        send_private_message(playersId[identityPos[civilian2]], "平民");
    } catch (ApiError&) {
    }
}

void groupData::sendPlayersPos(void)
{
    try {
        string str = "小白上帝上线~\n为你们编号如下:";

        for (int i = 0; i < numOfPlayers; i++) {
            str += '\n' + char(i+'1') + " : " + playersName[i];
        }

        send_group_message(groupId, str);
    } catch (ApiError&) {
    }
}

int groupData::getStep(void)
{
    return gameStep;
}

void groupData::startGame (void)
{
    for (int i = 0; i < numOfPlayers; i++)
        identityAlive[i] = 1;
    gameStep = 0;
    nextStep(false);
}

void groupData::nextStep(bool f)
{
    // step:
    // 0 initgame
    // 1 addplayers and send info
    // 2 night and wolf
    // 3 witch
    // 4 witch2
    // 5 hunter
    // 6 day, vote and kill someone or game over
    // 7 to 3/0
    try {
        if (f) gameStep++;
        switch (gameStep) {
            case 0: {
                send_group_message(groupId, "加入游戏请扣1~");
                break;
            }
            case 1: {
                sendPlayersPos();
                sendIdentity();
                break;
            }
            case 2: {
                memset(killId, -1, sizeof(killId));
                release = 0;
                send_group_message(groupId, "天黑请闭眼");
                send_group_message(groupId, "狼人请睁眼，私聊我你要击杀的对象。");
                if (identityAlive[wolf1])
                    send_private_message(playersId[identityPos[wolf1]], "击杀对象，一狼回复即可:");
                if (identityAlive[wolf2])
                    send_private_message(playersId[identityPos[wolf2]], "击杀对象，一狼回复即可:");
                break;
            }
            case 3: {
                send_group_message(groupId, "狼人请闭眼，女巫请睁眼。");
                send_group_message(groupId, "今天晚上他死了，你有一瓶解药是否要用？");
                Sleep(2000);
                send_group_message(groupId, "你有一瓶毒药是否要用？");
                if (identityAlive[witch]) {
                    string str = "今天晚上 " + char(killId+'1');
                    send_private_message(playersId[identityPos[witch]],  str + " 死了。");
                    if (antidote) {
                        send_private_message(playersId[identityPos[witch]], "你有一瓶解药是否要用（不使用发送0。使用发送1）？");
                        break;
                    } else {
                        send_private_message(playersId[identityPos[witch]], "你的解药已使用。");
                        Sleep(5000);
                        gameStep++; // 4
                    }
                } else {
                    Sleep(10000);
                    gameStep++; // 4
                }
            }
            case 4: {
                if (identityAlive[witch]) {
                    if (!release && poison) {
                        send_private_message(playersId[identityPos[witch]], "你有一瓶毒药是否要用（不使用发送0，使用发送使用对象编号）？");
                        break;
                    } else if (!poison) {
                        send_private_message(playersId[identityPos[witch]], "你的毒药已使用。");
                        Sleep(5000);
                    }
                } else {
                    Sleep(2000);
                }
                gameStep++; // 5
            }
            case 5: {
                send_group_message(groupId, "女巫请闭眼，猎人请睁眼。");
                if (identityAlive[hunter]) {
                    send_group_message(groupId, "你是否要带走一个人？");
                    send_private_message(playersId[identityPos[hunter]], "是否带走一个人（不带走发送0，否则发送对象编号）？");
                } else 
                    Sleep(6000);
                break;
            }
            case 6: {
                int diePos[3];
                memset(diePos, -1, sizeof(diePos));
                if (killId[0] != -1) {
                    diePos[0] = killId[0];
                    if (identityAlive[hunter] && killId[0] == identityPos[hunter]) {
                        diePos[2] = killId[2];
                    }
                }
                if (killId[1] != -1)
                    diePos[1] = killId[1];
                sort(diePos, diePos+3);
                int n = 0;
                while (n < 3 && diePos[n] == -1) n++;
                string ans = "";
                if (n < 3) {
                    for (; n < 3; n++) {
                        int idPos = 0;
                        for (int i = 0; i < numOfPlayers; i++) {
                            if (identityPos[i] == diePos[n]) {
                                idPos = i;
                                break;
                            }
                        }
                        identityAlive[idPos] = 0;
                        ans += "\n ";
                        ans += char(diePos[n] + '1') + " shi了";
                    }
                }
                if (ans.length() == 0)
                    ans = "昨天晚上没人shi亡";
                else
                    ans = "昨天晚上:" + ans;
                
                send_group_message(groupId, "天亮了~全员睁眼。");
                Sleep(1000);
                send_group_message(groupId, ans);
                break;
            }
            default: {
                logging::debug("wolf", "In function nextStep: Unexpected fall to default.");
                break;
            }
        }
        //if (f) gameStep++;
    } catch (ApiError&) {
    }
}

void groupData::getMessage(bool group, int64_t id, int n)
{
    try {
        switch (gameStep) {
            case 0: {
#ifdef WOLFDEBUG
    logging::debug("wolf", "getMessage case1 get: " + char(n+'0'));
#endif
                if (group && n == 1) {
                    int need = addPlayer(id);
                    if (need > 0) send_group_message(groupId, "还差: " + char(need + '0'));
                    else if (need < 0) logging::debug("wolf", "addplayer返回-1");
                    else {
                        send_group_message(groupId, "满员~");
                        nextStep(true); // 1
                        nextStep(true); // 2
                    }
                }
                break;
            }
            case 2: {
#ifdef WOLFDEBUG
    logging::debug("wolf", "getMessage case2 get: " + char(n+'0'));
#endif
                if (!group && 
                    (identityAlive[wolf1] && id == playersId[identityPos[wolf1]]) ||
                     identityAlive[wolf2] && id == playersId[identityPos[wolf2]]) {
                    int pos = n - 1;
                    if (n < 0 || n == identityPos[wolf1] || n == identityPos[wolf2]) {
                        send_private_message(id, "你是个狼人，你得鲨个人！");
                        break;
                    } else if (n < numOfPlayers) {
                        // alive? get pos in identityPos[]
                        int ipos = identityPos[wolf1];
                        for (int i = 0; i < numOfPlayers; i++) {
                            if (identityPos[i] == pos) {
                                ipos = i;
                                break;
                            }
                        }
                        if (!identityAlive[ipos]) {
                            send_private_message(id, "他已经shi了……你鲨不了他");
                            break;
                        }
                        killId[0] = pos;
                        send_private_message(id, "上帝已知晓你想鲨: " + char(pos + '1'));
                        //gameStep++;
                        nextStep(true); // 3
                        break;
                    } else {
                        send_private_message(id, "这个人不存在吧？");
                        break;
                    }
                }
                break;
            }
            case 3: {
                if (identityAlive[witch] && !group && antidote && id == playersId[identityPos[witch]]) {
                    if (n == 1) {
                        killId[0] = -1;
                        release = 1;
                        antidote = false;
                        send_private_message(id, "解救成功~");
                    } else if (n == 0)
                        send_private_message(id, "上帝已知晓你不想解救~");
                    if (n == 0 || n == 1)
                        nextStep(true); // 4
                }
                break;
            }
            case 4: {
                if (identityAlive[witch] && !group && poison && id == playersId[identityPos[witch]]) {
                    int killpos = n - 1;
                    if (killpos < -1 || killpos >= numOfPlayers)
                        send_private_message(id, "这个人不存在吧？");
                    else if (killpos == identityPos[witch])
                        send_private_message(id, "你想鲨你自己？不可~");
                    else if (killpos == -1) {
                        send_private_message(id, "上帝已知晓你将不使用毒药。");
                        nextStep(true); // 5
                    } else {
                        int identityp = identityPos[witch];
                        for (int i = 0; i < numOfPlayers; i++) {
                            if (identityPos[i] == killpos) {
                                identityp = i;
                                break;
                            }
                        }
                        if (!identityAlive[identityp]) {
                            send_private_message(id, "他已经shi了……你鲨不了他");
                            break;
                        }
                        killId[1] = killpos;
                        send_private_message(id, "上帝已知晓你想鲨: " + char(killpos + '1'));
                        nextStep(true); // 5
                    }
                }
                break;
            }
            case 5: {
                if (identityAlive[hunter] && !group && id == playersId[identityPos[hunter]]) {
                    int killpos = n-1;
                    if (killpos == identityPos[hunter])
                        send_private_message(id, "你不能带走自己~");
                    else if (killpos < -1 || killpos >= numOfPlayers)
                        send_private_message(id, "这好像不是个人。");
                    else if (killpos == -1) {
                        send_private_message(id, "上帝已知晓你不想带走人~");
                        nextStep(true); // 6
                    } else {
                        int killposn = identityPos[hunter];
                        for (int i = 0; i < numOfPlayers; i++) {
                            if (identityPos[i] == killpos) {
                                killposn = i;
                                break;
                            }
                        }
                        if (identityAlive[killposn]) {
                            send_private_message(id, "你不能带走一个shi人~");
                            break;
                        }
                        killId[2] = killpos;
                        nextStep(true);
                    }
                }
                break;
            }
            case 6: {
                break;
            }
            default: {
                logging::debug("wolf", "In function getMessage: Unexpected fall to default.");
                break;
            }
        }
    } catch (ApiError&) {
    }
}

class CMainData {
private:
    static map<int64_t, class groupData> groupMap;
};

CQ_INIT {
    on_enable([] {
        /*logging::info("启用", "wolf启用后台");
        srand(time(NULL));
        struct tm *lct;
        int clearn = 0;
        while (1) {
            gettime(lct);
            //lct->tm_isdst = 0; //Daylight Saving Time flag
            if (lct->tm_hour == 4 && lct->tm_min == 0) {
                if (!clearn) func(0), clearn = 1; 
            } else {
                clearn = 0;
            }
            Sleep(60000);
        }*/
    });

    on_group_message([](const GroupMessageEvent &event) {
        try {
            /*if (CmainData::run) {
                if (CmainData::repeatMsg(event.group_id, event.message))
                    send_group_message(event.group_id, event.message);
            }*/
        } catch (ApiError&) {
        }
    });

    on_private_message([](const PrivateMessageEvent &event) {
        try {
            /*if (event.message == ".status") {
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
            }*/
        } catch (ApiError &) {
        }
    });
}

