#include "Recruitment.h"
#include "../Reward/Reward.h"
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

void AddSC_recruitment_npc()
{
	new Recruitment_npc();
}
