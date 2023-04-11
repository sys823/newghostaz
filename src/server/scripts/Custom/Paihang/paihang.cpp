#include "paihang.h"
#include "../Reward/Reward.h"
#include "../CommonFunc/CommonFunc.h"
#include "../Switch/Switch.h"
#include <fstream>
#include <iostream>

void PaiHang::Load()
{
    Vpaihangconf.clear();
    int nCount = 0;
    maxpaihang = 0;
	QueryResult result = WorldDatabase.PQuery("SELECT 名次,积分榜BUFF,杀人榜BUFF,在线榜BUFF,积分榜上线公告,杀人榜上线公告,在线榜上线公告 FROM _排行榜配置");
	if (result)
    {
		do
		{
            paihangconf tmp;
			Field* fields = result->Fetch();
            tmp.minci = fields[0].GetUInt32();
            tmp.jf_buff = fields[1].GetUInt32();
            tmp.kill_buff = fields[2].GetUInt32();
            tmp.on_buff = fields[3].GetUInt32();
            tmp.jf_gonggao = fields[4].GetString();
            tmp.kill_gonggao = fields[5].GetString();
            tmp.on_gonggao = fields[6].GetString();

            if (maxpaihang < tmp.minci)
                maxpaihang = tmp.minci;
            
            Vpaihangconf.insert(paihangconf_t::value_type(tmp.minci,tmp));
            nCount++;

		} while (result->NextRow());
	}
    sLog->outString(">> 读取自定义功能数据表 _排行榜配置 ,共%u条数据读取加载...", nCount);

    SendPaiHangUpdate();
}

void PaiHang::StartEvent(uint32 eventid)
{
    if (eventid == sSwitch->GetValue(PAIHANG_EVENTID))
    {
        SendPaiHangUpdate();

        SessionMap const& smap = sWorld->GetAllSessions();
        for (SessionMap::const_iterator itr = smap.begin(); itr != smap.end(); ++itr)
            if (Player* player = itr->second->GetPlayer())
                if (player->IsInWorld())
                    SendAndClearBufforGG(player, false);
    }

    uint32 day_jf_event = sSwitch->GetValueByIndex(JF_CZ_SC, 3);
    if (day_jf_event == eventid)
    {
        LoginDatabase.PExecute("update account set jf_day = 0");
    }
}

void PaiHang::SendPaiHangUpdate()
{
    VCph_kill.clear();
    VCph_time.clear();
    VCph_jf.clear();

    QueryResult resultjf = CharacterDatabase.Query("select guid, totalTokenAmount from characters order by totalTokenAmount desc limit 10");
    if (resultjf)
    {
        int nCount = 1;
        do
        {
            Field *fields = resultjf->Fetch();
            if (fields[1].GetUInt32())
            {
                std::ostringstream mydis;
                mydis << fields[0].GetUInt32() << "," << fields[1].GetUInt32();
                VCph_jf[nCount] = mydis.str();
                nCount++;
            }
        } while (resultjf->NextRow());
    }

    QueryResult resultkill = CharacterDatabase.Query("select guid, totalKills from characters order by totalKills desc limit 10");
    if (resultkill)
    {
        int nCount = 1;
        do
        {
            Field *fields = resultkill->Fetch();
            if (fields[1].GetUInt32())
            {
                std::ostringstream mydis;
                mydis << fields[0].GetUInt32() << "," << fields[1].GetUInt32();
                VCph_kill[nCount] = mydis.str();
                nCount++;
            }
        } while (resultkill->NextRow());
    }

    QueryResult resulttime = CharacterDatabase.Query("select  guid, totaltime from characters order by totaltime desc limit 10");
    if (resulttime)
    {
        int nCount = 1;
        do
        {
            Field *fields = resulttime->Fetch();
            if (fields[1].GetUInt32())
            {
                std::ostringstream mydis;
                mydis << fields[0].GetUInt32() << "," << fields[1].GetUInt32();
                VCph_time[nCount] = mydis.str();
                nCount++;
            }
        } while (resulttime->NextRow());
    }
}

void PaiHang::SendAndClearBufforGG(Player * pl, bool gonggao)
{
    for (uint8 i = 1; i <= maxpaihang; i++)
    {
        const paihangconf * pconf = Findpaihang(i);
        if (pconf)
        {
            if (VCph_jf.size() && pconf->jf_buff)
            {
                std::string data = VCph_jf[i];
                Tokenizer tokens(data, ',', 2);
                uint32 guid = atol(tokens[0]);
                if (pl->GetGUIDLow() == guid && !pl->HasAura(pconf->jf_buff))
                    pl->AddAura(pconf->jf_buff, pl);
                else if (pl->GetGUIDLow() != guid && pl->HasAura(pconf->jf_buff))
                    pl->RemoveAurasDueToSpell(pconf->jf_buff);

                if (gonggao && pl->GetGUIDLow() == guid && !pconf->jf_gonggao.empty())
                {
                    sWorld->SendGlobalText(G3D::format(pconf->jf_gonggao.c_str(), sCF->GetNameLink(pl).c_str()).c_str(), NULL);
                }
            }

            if (VCph_kill.size() && pconf->kill_buff)
            {
                std::string data = VCph_kill[i];
                Tokenizer tokens(data, ',', 2);
                uint32 guid = atol(tokens[0]);
                if (pl->GetGUIDLow() == guid && !pl->HasAura(pconf->kill_buff))
                    pl->AddAura(pconf->kill_buff, pl);
                else if (pl->GetGUIDLow() != guid && pl->HasAura(pconf->kill_buff))
                    pl->RemoveAurasDueToSpell(pconf->kill_buff);

                if (gonggao && pl->GetGUIDLow() == guid && !pconf->kill_gonggao.empty())
                {
                    sWorld->SendGlobalText(G3D::format(pconf->kill_gonggao.c_str(), sCF->GetNameLink(pl).c_str()).c_str(), NULL);
                }
            }

            if (VCph_time.size() && pconf->on_buff)
            {
                std::string data = VCph_time[i];
                Tokenizer tokens(data, ',', 2);
                uint32 guid = atol(tokens[0]);
                if (pl->GetGUIDLow() == guid && !pl->HasAura(pconf->on_buff))
                    pl->AddAura(pconf->on_buff, pl);
                else if (pl->GetGUIDLow() != guid && pl->HasAura(pconf->on_buff))
                    pl->RemoveAurasDueToSpell(pconf->on_buff);

                if (gonggao && pl->GetGUIDLow() == guid && !pconf->on_gonggao.empty())
                {
                    sWorld->SendGlobalText(G3D::format(pconf->on_gonggao.c_str(), sCF->GetNameLink(pl).c_str()).c_str(), NULL);
                }
            }
        }
    }
}

void PaiHang::AddPaiHangList(Player* pPlayer, Object* obj, uint32 typesid)
{
    uint32 sender = 9004;
    if (typesid == 1) //击杀排行
    {
        pPlayer->PlayerTalkClass->ClearMenus();

        if (VCph_kill.size())
        {
            for (int i = 1; i < VCph_kill.size() + 1; i++)
            {
                if (i > 10)
                    continue;

                std::string data = VCph_kill[i];
                uint32 guid = 0;
                uint32 killcount = 0;
                Tokenizer tokens(data, ',', 2);

                guid = atol(tokens[0]);
                killcount = atol(tokens[1]);

                if (guid)
                {
                    GlobalPlayerData const* playerData = sWorld->GetGlobalPlayerData(guid);
                    if (playerData)
                    {
                        std::ostringstream hsfdata;

                        hsfdata << "第" << i << "名;" << playerData->name.c_str() << ",击杀: " << killcount;

                        pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, hsfdata.str().c_str(), sender, 10);
                    }
                }
            }
        }
        else
            pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, "暂无击杀排行", sender, 10);

        pPlayer->SEND_GOSSIP_MENU(20000, obj->GetGUID());

    }
    else if (typesid == 2) //在线排行
    {
        pPlayer->PlayerTalkClass->ClearMenus();

        if (VCph_time.size())
        {
            for (int i = 1; i < VCph_time.size() + 1; i++)
            {
                if (i > 10)
                    continue;

                std::string data = VCph_time[i];
                uint32 guid = 0;
                uint32 killcount = 0;
                Tokenizer tokens(data, ',', 2);

                guid = atol(tokens[0]);
                killcount = atol(tokens[1]);
                std::string intimes = secsToTimeString(killcount, true);
                if (guid)
                {
                    GlobalPlayerData const* playerData = sWorld->GetGlobalPlayerData(guid);
                    if (playerData)
                    {
                        std::ostringstream hsfdata;

                        hsfdata << "第" << i << "名;" << playerData->name.c_str() << ",在线时长: " << intimes.c_str();

                        pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, hsfdata.str().c_str(), sender, 10);
                    }
                }
            }
        }
        else
            pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, "暂无在线排行", sender, 10);

        pPlayer->SEND_GOSSIP_MENU(20000, obj->GetGUID());
    }
    else if (typesid == 3)
    {
        pPlayer->PlayerTalkClass->ClearMenus();

        if (VCph_jf.size())
        {
            for (int i = 1; i < VCph_jf.size() + 1; i++)
            {
                if (i > 10)
                    continue;

                std::string data = VCph_jf[i];
                uint32 guid = 0;
                uint32 jfcost = 0;
                Tokenizer tokens(data, ',', 2);
                guid = atol(tokens[0]);
                jfcost = atol(tokens[1]);
                if (guid)
                {
                    GlobalPlayerData const* playerData = sWorld->GetGlobalPlayerData(guid);
                    if (playerData)
                    {
                        std::ostringstream hsfdata;

                        hsfdata << "第" << i << "名;" << playerData->name.c_str() << ",消耗财富:" << jfcost;

                        pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, hsfdata.str().c_str(), sender, 10);
                    }
                }
            }
        }
        else
            pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, "暂无财富排行", sender, 10);

        pPlayer->SEND_GOSSIP_MENU(20000, obj->GetGUID());
    }
}

void PaiHang::LQJF(Player* player)
{
    QueryResult result = LoginDatabase.PQuery("SELECT jf_new,jf_one,jf_day from account where id= %u", player->GetSession()->GetAccountId());
    if (result)
    {
        uint32 jf_new = result->Fetch()[0].GetUInt32();
        int32 jf_one = result->Fetch()[1].GetInt32();
        int32 jf_day = result->Fetch()[2].GetInt32();
        //============================================
        if (jf_new == 0)
        {
            player->GetSession()->SendNotification("请在网站上充值后在领取积分");
        }
        else
        {
            sCF->UpdateTokenAmount(player, jf_new, true, "网站充值领取积分");
            player->GetSession()->SendNotification("充值的积分%u已到账", jf_new);

            //==========首次充值奖励==============
            uint32 one_jf = sSwitch->GetValueByIndex(JF_CZ_SC, 1);
            uint32 one_jf_item = sSwitch->GetValueByIndex(JF_CZ_SC, 2);
            if (one_jf && one_jf_item)
            {
                if (jf_one >= 0)
                {
                    LoginDatabase.PExecute("update account set jf_one = jf_one + %u where id =%u", jf_new, player->GetSession()->GetAccountId());
                    if (jf_one + jf_new >= one_jf) //达到目标
                    {
                        sRew->RewItem(player, one_jf_item, 1);
                        LoginDatabase.PExecute("update account set jf_one = -1 where id =%u", player->GetSession()->GetAccountId());
                    }
                    else
                    {
                        ItemTemplate const * ipt = sObjectMgr->GetItemTemplate(one_jf_item);
                        uint32 haicha = one_jf - jf_one - jf_new;
                        ChatHandler(player->GetSession()).PSendSysMessage("继续充值%u积分将会获得%s首充奖励", haicha, ipt->GetINameLink(0, 0).c_str());
                    }
                }
            }
            //=========每日充值奖励=====================
            uint32 day_jf = sSwitch->GetValueByIndex(JF_CZ_SC, 1);
            uint32 day_jf_item = sSwitch->GetValueByIndex(JF_CZ_SC, 2);

            if (day_jf && day_jf_item)
            {
                if (jf_day >= 0)
                {
                    LoginDatabase.PExecute("update account set jf_day = jf_day + %u where id =%u", jf_new, player->GetSession()->GetAccountId());
                    if (jf_day + jf_new >= day_jf) //达到目标
                    {
                        sRew->RewItem(player, day_jf_item, 1);
                        LoginDatabase.PExecute("update account set jf_day = -1 where id =%u", player->GetSession()->GetAccountId());
                    }
                    else
                    {
                        ItemTemplate const * ipt = sObjectMgr->GetItemTemplate(day_jf_item);
                        uint32 haichaa = day_jf - jf_day - jf_new;
                        ChatHandler(player->GetSession()).PSendSysMessage("继续充值%u积分将会获得%s日充奖励", haichaa, ipt->GetINameLink(0, 0).c_str());
                    }
                }
            }
        }
    }
    else
    {
        player->GetSession()->SendNotification("未知错误..");
    }
    player->CLOSE_GOSSIP_MENU();
}
