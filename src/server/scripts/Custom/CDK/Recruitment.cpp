#include "Recruitment.h"
#include "../Reward/Reward.h"
#include "../Requirement/Requirement.h"
#include "../Switch/Switch.h"
#include <fstream>
#include <iostream>
#include "World.h"

void Recruitment::LoadWorld()
{
	if (!m_RecruitmentRws.empty())
	{
		for (CRecruitmentRwContainer::iterator itr = m_RecruitmentRws.begin(); itr != m_RecruitmentRws.end(); ++itr)
			delete itr->second;

		m_RecruitmentRws.clear();
	}
    /*
	QueryResult result = WorldDatabase.PQuery("SELECT 等级, 硬核模式,奖励模板ID,招募奖励模板ID FROM _招募奖励");
	if (result)
	{
		do
		{
			Field* fields = result->Fetch();

			RecruitmentRw * worldsys = new RecruitmentRw;
			worldsys->level = fields[0].GetUInt8();
			worldsys->checkone = fields[1].GetUInt8();
			worldsys->rewid = fields[2].GetUInt32();
			worldsys->brewid = fields[3].GetUInt32();

			uint16 key_ID = MAKE_PAIR16(worldsys->level, worldsys->checkone);
			m_RecruitmentRws[key_ID] = worldsys;

		} while (result->NextRow());
	}
    */
    m_DmgPhrewMaps.clear();

    QueryResult result = WorldDatabase.PQuery("SELECT 伤害排名, 奖励模板ID FROM _伤害_排行奖励");
    if (result)
    {
        do
        {
            DmgPhrew *Temp = new DmgPhrew;
            Field* fields = result->Fetch();
            Temp->phid = fields[0].GetUInt32();

            Tokenizer auraspells(fields[1].GetString(), '#');
            if (auraspells.size())
            {
                for (uint32 index = 0; index < auraspells.size(); ++index)
                    Temp->rewids[index] = atoi(auraspells[index]);
            }
            m_DmgPhrewMaps.push_back(Temp);

        } while (result->NextRow());
    }
}

uint32 Recruitment::GetDmgRewID(uint32 phid, uint32 guidlow)
{
    DmgPhrew * phrewsys = FindAuraOnDeathMod(phid);
    if (!phrewsys)
        return 0;

    uint8 classid = 0;
    GlobalPlayerData const* playerData = sWorld->GetGlobalPlayerData(guidlow);
    if (playerData)
        classid = playerData->playerClass;

    if (!classid)
        return 0;

    if (classid == 11)
        classid = 10;

    return phrewsys->rewids[classid - 1];
}

void Recruitment::Create(Player * player, std::string plname) //招募玩家
{
	uint32 guid = sWorld->GetGlobalPlayerGUID(plname);
	if (!guid)
	{
		ChatHandler(player->GetSession()).PSendSysMessage("[招募系统]：请输入正确的玩家名字！");
		return;
	}

    GlobalPlayerData const* playerData = sWorld->GetGlobalPlayerData(guid);
    if (playerData->accountId == player->GetSession()->GetAccountId())
    {
        ChatHandler(player->GetSession()).PSendSysMessage("[招募系统]：不能设置自己账号下的角色！");
        return;
    }

	sWorld->UpdateGlobalPlayerRecruitment(player->GetGUIDLow(), guid);
    CharacterDatabase.PExecute("update characters set recruitmentguid = %u where guid = %u", guid, player->GetGUIDLow());
	RecruitmentRw * rewsys = GetReParameters(0, 0);
	if (rewsys)
	{
		if (rewsys->rewid)
			sRew->Rew(player, rewsys->rewid);

		if (rewsys->brewid)
			SendRewardMarkByMail(player, guid, rewsys->brewid);
	}
	
}

void Recruitment::ForPlayerLevelup(Player * player, uint8 level)
{
    return;
	GlobalPlayerData const* playerData = sWorld->GetGlobalPlayerData(player->GetGUIDLow());
	if (!playerData)
	{
		//ChatHandler(player->GetSession()).PSendSysMessage("[招募系统]：错误！");
		return;
	}

	if (!playerData->Recruitmentguid)
	{
		ChatHandler(player->GetSession()).PSendSysMessage("[招募系统]：没有设置招募队友无奖励！");
		return;
	}

	RecruitmentRw * rewsys = GetReParameters(level, 0);
	if (rewsys)
	{
		if (rewsys->rewid)
			sRew->Rew(player, rewsys->rewid);

		if (rewsys->brewid)
			SendRewardMarkByMail(player, playerData->Recruitmentguid, rewsys->brewid);
	}
}

void Recruitment::SendRewardMarkByMail(Player * plr, uint32 guid, uint32 rewId)
{
    std::unordered_map<uint32, RewTemplate>::iterator iter = RewMap.find(rewId);
	if (iter != RewMap.end())
	{
		typedef std::pair<uint32, uint32> ItemPair;
		typedef std::list< ItemPair > ItemPairs;
		ItemPairs items;
		std::string pname = plr->GetName();
		std::string itemText = "你通过招募 " + pname + " 获得奖励";
		std::string subject = "招募奖励.";

		for (auto itr = iter->second.ItemDataVec.begin(); itr != iter->second.ItemDataVec.end(); itr++)
		{
			ItemTemplate const* markProto = sObjectMgr->GetItemTemplate(itr->itemId);
			if (!markProto)
				continue;
			uint32 count = itr->itemCount;

			while (count > markProto->GetMaxStackSize())
			{
				items.push_back(ItemPair(itr->itemId, markProto->GetMaxStackSize()));
				count -= markProto->GetMaxStackSize();
			}

			items.push_back(ItemPair(itr->itemId, count));
		}

		MailDraft draft(subject, itemText);

		SQLTransaction trans = CharacterDatabase.BeginTransaction();

		for (ItemPairs::const_iterator itr = items.begin(); itr != items.end(); ++itr)
		{
			if (Item* item = Item::CreateItem(itr->first, itr->second, plr))
			{
				item->SaveToDB(trans);
				draft.AddItem(item);
			}
		}

		uint64 rc = MAKE_NEW_GUID(guid, 0, HIGHGUID_PLAYER);
		Player* receive = ObjectAccessor::FindPlayerInOrOutOfWorld(rc);
		draft.SendMailTo(trans, MailReceiver(receive, GUID_LOPART(rc)), MailSender(MAIL_NORMAL, 0, MAIL_STATIONERY_GM));
		CharacterDatabase.CommitTransaction(trans);
	}
}

void Recruitment::SendDmgPhByMail(uint32 guid, uint32 rewId, uint32 phid)
{
    std::unordered_map<uint32, RewTemplate>::iterator iter = RewMap.find(rewId);
    if (iter != RewMap.end())
    {
        typedef std::pair<uint32, uint32> ItemPair;
        typedef std::list< ItemPair > ItemPairs;
        ItemPairs items;

        std::ostringstream oos;
        oos << "你在本职业伤害排行中 第[" << phid << "名] 获得奖励";

        std::string itemText = oos.str().c_str();
        std::string subject = "伤害排行奖励.";

        for (auto itr = iter->second.ItemDataVec.begin(); itr != iter->second.ItemDataVec.end(); itr++)
        {
            ItemTemplate const* markProto = sObjectMgr->GetItemTemplate(itr->itemId);
            if (!markProto)
                continue;
            uint32 count = itr->itemCount;

            while (count > markProto->GetMaxStackSize())
            {
                items.push_back(ItemPair(itr->itemId, markProto->GetMaxStackSize()));
                count -= markProto->GetMaxStackSize();
            }

            items.push_back(ItemPair(itr->itemId, count));
        }

        MailDraft draft(subject, itemText);

        SQLTransaction trans = CharacterDatabase.BeginTransaction();

        for (ItemPairs::const_iterator itr = items.begin(); itr != items.end(); ++itr)
        {
            if (Item* item = Item::CreateItem(itr->first, itr->second))
            {
                item->SaveToDB(trans);
                draft.AddItem(item);
            }
        }

        uint64 rc = MAKE_NEW_GUID(guid, 0, HIGHGUID_PLAYER);
        Player* receive = ObjectAccessor::FindPlayerInOrOutOfWorld(rc);
        draft.SendMailTo(trans, MailReceiver(receive, GUID_LOPART(rc)), MailSender(MAIL_NORMAL, 0, MAIL_STATIONERY_GM));
        CharacterDatabase.CommitTransaction(trans);
    }
}

class Recruitment_npc : public CreatureScript
{
public:
	Recruitment_npc() : CreatureScript("Recruitment_npc")
	{
	}

	bool OnGossipHello(Player* player, Creature* creature) override
	{
		player->PlayerTalkClass->ClearMenus();

		GlobalPlayerData const* playerData = sWorld->GetGlobalPlayerData(player->GetGUIDLow());
		if (!playerData)
			return true;

		if (playerData->Recruitmentguid) //已经写了招募老玩家名字
		{
			GlobalPlayerData const* plData = sWorld->GetGlobalPlayerData(playerData->Recruitmentguid);
			if (plData)
			{
				player->ADD_GOSSIP_ITEM(GOSSIP_ICON_DOT, " [招募系统] ", 1000, 1);
				player->ADD_GOSSIP_ITEM(GOSSIP_ICON_DOT, " 你的上线: " + plData->name, 1000, 1);
			}
			
		}
		else
		{
			player->ADD_GOSSIP_ITEM(GOSSIP_ICON_DOT, " [招募系统] ", 1000, 1);
			player->ADD_GOSSIP_ITEM_EXTENDED(GOSSIP_ICON_MONEY_BAG, "|TInterface/Icons/INV_BannerPVP_01:30:30:0:0|t[招募系统]写入玩家名字 ", 1000, 2, "在弹窗的电子兑换码窗体中输入玩家的名字", 0, true);
		}

		player->SEND_GOSSIP_MENU(20000, creature->GetGUID());
		return true;
	}

	bool OnGossipSelectCode(Player* player, Creature* creature, uint32 sender, uint32 action, char const* code)
	{
		if (action == 1)
		{
			OnGossipHello(player, creature);
		}
		else if (action == 2)
		{
			sRecruitment->Create(player, code);
            OnGossipHello(player, creature);
		}
		
		return true;
	}
};

void Recruitment::ResetAllDmg(uint32 eventid)
{
    if (eventid != sSwitch->GetValue(MY_DMG_PH_EVENTID))
        return;

    for (int classid = 1; classid < 12; classid++)
    {
        for (int i = 0; i < sRecruitment->allmaps[classid].size(); i++)
        {
            DmgPhrew * ttt = FindAuraOnDeathMod(i + 1);
            if (!ttt)
                continue;

            uint32 plguid = sRecruitment->allmaps[classid][i];
            uint32 rewid = GetDmgRewID(i + 1, plguid);
            if (rewid)
            {
                SendDmgPhByMail(plguid, rewid, i + 1);
            }
        }
    }


    //根据排行给奖励
    for (int classid1 = 1; classid1 < 12; classid1++)
        allmaps[classid1].clear();
    dmgmaps.clear();

    CharacterDatabase.PExecute("update characters set dmgph = 0");
}

uint32 Recruitment::GetClassDmgPH(Player * player)
{
    if (dmgmaps[player->GetGUIDLow()]==0)
        return 0;

    for (int i = 0; i < sRecruitment->allmaps[player->getClass()].size(); i++)
    {
        if (sRecruitment->allmaps[player->getClass()][i] == player->GetGUIDLow())
            return i + 1;
    }

    return 0;
}

void Recruitment::SortAllDmg()
{
    for (int classid = 1; classid < 12; classid++)
        allmaps[classid].clear();

    for (auto &[key, value] : dmgmaps)
    {
        GlobalPlayerData const* playerData = sWorld->GetGlobalPlayerData(key);
        if (value > 0)
            allmaps[playerData->playerClass].emplace_back(key);
    }

    for (int classid = 1; classid < 12; classid++)
    {
        if (!allmaps[classid].size())
            continue;

        sort(allmaps[classid].begin(), allmaps[classid].end(), [&](const uint32&a, const uint32&b)->bool {
            return dmgmaps[a] > dmgmaps[b];
            });
    }
}

void Recruitment::SortClassDmg(uint8 classid, uint32 guid, uint64 dmg)
{
    allmaps[classid].clear();

    for (auto &[key, value] : dmgmaps)
    {
        GlobalPlayerData const* playerData = sWorld->GetGlobalPlayerData(key);
        if (value > 0 && playerData->playerClass == classid)
            allmaps[playerData->playerClass].emplace_back(key);
    }

    sort(allmaps[classid].begin(), allmaps[classid].end(), [&](const uint32&a, const uint32&b)->bool {
        return dmgmaps[a] > dmgmaps[b];
        });

}

std::string Recruitment::GetClass(uint8 playerClass)
{
    switch (playerClass)
    {
    case 1:
        return "战士";
    case 2:
        return "圣骑士";
    case 3:
        return "猎人";
    case 4:
        return "盗贼";
    case 5:
        return "牧师";
    case 6:
        return "死亡骑士";
    case 7:
        return "萨满祭司";
    case 8:
        return "法师";
    case 9:
        return "术士";
    case 11:
        return "德鲁伊";
    default:
        break;
    }
    return "";
}

class npc_dmg_dummy : public CreatureScript
{
public:
    npc_dmg_dummy() : CreatureScript("npc_dmg_dummy") { }

    
    struct npc_dmg_dummyAI : ScriptedAI
    {
        npc_dmg_dummyAI(Creature* creature) : ScriptedAI(creature)
        {
            SetCombatMovement(false);
            me->ApplySpellImmune(0, IMMUNITY_EFFECT, SPELL_EFFECT_KNOCK_BACK, true); //imune to knock aways like blast wave
        }
        uint32 playerguid;
        uint8 playclassid;
        uint32 starttime = 0;
        uint32 endtime = 0;
        uint32 resetTimer;
        uint64 alldamge;
        uint32 mymenuid = 0;
        void Reset()
        {
            me->CastSpell(me, 61204, true);
            resetTimer = 5000;
        }

        void EnterEvadeMode()
        {
            if (!_EnterEvadeMode())
                return;

            Reset();
        }

        void DamageTaken(Unit* unit, uint32& damage, DamageEffectType, SpellSchoolMask)
        {
            if (Player * ishello = unit->GetCharmerOrOwnerPlayerOrPlayerItself())
            {
                if (ishello->GetGUIDLow() == playerguid)
                    alldamge += damage;
            }
            
            resetTimer = 5000;
            damage = 0;
        }

        void UpdateAI(uint32 diff)
        {
            if (starttime)
            {
                if (starttime <= diff)
                {
                    me->RemoveFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP);
                    me->setFaction(7);
                    starttime = 0;
                    alldamge = 0;
                    endtime = sSwitch->GetValue(MY_DMG_PH_TIIME) * 1000;

                    GlobalPlayerData const* playerData = sWorld->GetGlobalPlayerData(playerguid);
                    std::ostringstream oss;
                    oss << "勇士[" << playerData->name << "]开始挑战!挑战时间为" << sSwitch->GetValue(MY_DMG_PH_TIIME) << "秒!!!";
                    me->MonsterYell(oss.str().c_str(), LANG_UNIVERSAL, NULL);
                }
                else
                {
                    uint32 newtime = starttime - diff;

                    if (newtime / IN_MILLISECONDS != starttime / IN_MILLISECONDS)
                    {
                        std::ostringstream oss;
                        oss << "将在" << uint32(starttime / 1000) << "秒后开始战斗";
                        me->MonsterSay(oss.str().c_str(), LANG_UNIVERSAL, NULL);
                    }
                    starttime = newtime;
                }
            }

            if (endtime)
            {
                if (endtime <= diff)
                {
                    me->setFaction(35);
                    me->SetFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP);

                    GlobalPlayerData const* playerData = sWorld->GetGlobalPlayerData(playerguid);
                    std::ostringstream oss;
                    oss << "勇士[" << playerData->name << "]挑战结束,总伤害:" << alldamge;
                    me->MonsterYell(oss.str().c_str(), LANG_UNIVERSAL, NULL);

                    endtime = 0;
                    mymenuid = 0;
                    if (sRecruitment->dmgmaps[playerguid] < alldamge)
                    {
                        sRecruitment->dmgmaps[playerguid] = alldamge;
                        sRecruitment->SortClassDmg(playclassid, playerguid, alldamge);
                        CharacterDatabase.PExecute("update characters set dmgph = %llu where guid = %u", alldamge, playerguid);
                    }
                }
                else
                    endtime -= diff;
            }

            if (!UpdateVictim())
                return;

            if (resetTimer <= diff)
            {
                EnterEvadeMode();
                resetTimer = 5000;
            }
            else
                resetTimer -= diff;
        }

        void MoveInLineOfSight(Unit* /*who*/) { }

        void sGossipHello(Player* player) override
        {
            mymenuid = 1;
            player->PlayerTalkClass->ClearMenus();
            player->ADD_GOSSIP_ITEM(GOSSIP_ICON_DOT, " [伤害挑战] ", 1000, 1);
            player->ADD_GOSSIP_ITEM(GOSSIP_ICON_DOT, " [伤害排行] ", 1000, 2);
            player->SEND_GOSSIP_MENU(20000, me->GetGUID());
        }

        void sGossipSelect(Player* player, uint32 menuid, uint32 gossipListId) override
        {
            if (mymenuid == 0)
            {
                sGossipHello(player);
            }
            else if (mymenuid == 1)
            {
                if (gossipListId == 0)
                {
                    uint32 reqid = sSwitch->GetValue(MY_DMG_PH);
                    if (!sReq->Check(player, reqid))
                    {
                        sGossipHello(player);
                        return;
                    }
                    playerguid = player->GetGUIDLow();
                    playclassid = player->getClass();
                    player->PlayerTalkClass->SendCloseGossip();
                    sReq->Des(player, reqid);
                    starttime = 3000;
                    me->MonsterSay("哈哈哈 准备战斗吧", LANG_UNIVERSAL, NULL);
                }
                else if (gossipListId == 1)
                {
                    mymenuid = 2;
                    player->PlayerTalkClass->ClearMenus();
                    player->ADD_GOSSIP_ITEM(GOSSIP_ICON_DOT, " [战士] ", 1001, 1);
                    player->ADD_GOSSIP_ITEM(GOSSIP_ICON_DOT, " [圣骑士] ", 1001, 2);
                    player->ADD_GOSSIP_ITEM(GOSSIP_ICON_DOT, " [猎人] ", 1001, 3);
                    player->ADD_GOSSIP_ITEM(GOSSIP_ICON_DOT, " [潜行者] ", 1001, 4);
                    player->ADD_GOSSIP_ITEM(GOSSIP_ICON_DOT, " [牧师] ", 1001, 5);
                    player->ADD_GOSSIP_ITEM(GOSSIP_ICON_DOT, " [死亡骑士] ", 1001, 6);
                    player->ADD_GOSSIP_ITEM(GOSSIP_ICON_DOT, " [萨满] ", 1001, 8);
                    player->ADD_GOSSIP_ITEM(GOSSIP_ICON_DOT, " [法师] ", 1001, 8);
                    player->ADD_GOSSIP_ITEM(GOSSIP_ICON_DOT, " [术士] ", 1001, 8);
                    player->ADD_GOSSIP_ITEM(GOSSIP_ICON_DOT, " [德鲁伊] ", 1001, 8);
                    player->SEND_GOSSIP_MENU(20000, me->GetGUID());
                }
            }
            else if (mymenuid == 2)
            {
                mymenuid = 0;
                player->PlayerTalkClass->ClearMenus();
                uint8 classid = 0;

                if (gossipListId < 9)
                    classid = gossipListId + 1;
                else
                    classid = gossipListId + 2;

                std::ostringstream ooos;
                ooos << " [" << sRecruitment->GetClass(classid) << "伤害排行] ";
                
                player->ADD_GOSSIP_ITEM(GOSSIP_ICON_DOT, ooos.str().c_str(), 1000, 1);
                if (!sRecruitment->allmaps[classid].size())
                    player->ADD_GOSSIP_ITEM(GOSSIP_ICON_DOT, " [暂无排行] ", 1000, 1);

                bool isinpaihang = false;
                for (int i = 0; i < sRecruitment->allmaps[classid].size(); i++)
                {
                    if (i >= 20)
                        break;

                    if (sRecruitment->allmaps[classid][i] == player->GetGUIDLow())
                        isinpaihang = true;

                    GlobalPlayerData const* playerData = sWorld->GetGlobalPlayerData(sRecruitment->allmaps[classid][i]);
                    std::ostringstream oos;
                    oos << "第" << i + 1 << "名: [" << playerData->name << "]  伤害:" << sRecruitment->dmgmaps[sRecruitment->allmaps[classid][i]];
                    player->ADD_GOSSIP_ITEM(GOSSIP_ICON_DOT, oos.str().c_str(), 1000, 1);
                }

                if (!isinpaihang)
                {
                    if (player->getClass() == classid)
                    {
                        uint32 yourph = sRecruitment->GetClassDmgPH(player);
                        if (yourph > 21)
                        {
                            player->ADD_GOSSIP_ITEM(GOSSIP_ICON_DOT, ".", 1000, 1);
                            player->ADD_GOSSIP_ITEM(GOSSIP_ICON_DOT, ".", 1000, 1);
                            player->ADD_GOSSIP_ITEM(GOSSIP_ICON_DOT, ".", 1000, 1);
                            player->ADD_GOSSIP_ITEM(GOSSIP_ICON_DOT, ".", 1000, 1);
                            player->ADD_GOSSIP_ITEM(GOSSIP_ICON_DOT, ".", 1000, 1);
                        }
                        if (yourph)
                        {
                            std::ostringstream oos;
                            oos << "第" << yourph << "名: [" << player->GetName() << "]  伤害:" << sRecruitment->dmgmaps[player->GetGUIDLow()];
                            player->ADD_GOSSIP_ITEM(GOSSIP_ICON_DOT, oos.str().c_str(), 1000, 1);
                        }
                    }
                }
                player->SEND_GOSSIP_MENU(20000, me->GetGUID());

            }  
        }
    };

    CreatureAI* GetAI(Creature* creature) const
    {
        return new npc_dmg_dummyAI(creature);
    }
};

void Recruitment::loadAllChar()
{
    PreparedStatement* stmt = CharacterDatabase.GetPreparedStatement(CHAR_SEL_DMG_PH);
    PreparedQueryResult result = CharacterDatabase.Query(stmt);
    if (!result)
        return;

    do
    {
        Field* fields = result->Fetch();
        dmgmaps[fields[0].GetUInt32()] = fields[1].GetUInt64();
    } while (result->NextRow());

    SortAllDmg();
}

void AddSC_recruitment_npc()
{
	new Recruitment_npc();
    new npc_dmg_dummy();
}
