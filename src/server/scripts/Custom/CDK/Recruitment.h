struct RecruitmentRw
{
	uint8 level;
	int8 checkone;
	uint32 rewid;
	uint32 brewid;
};

struct DmgPhrew
{
    uint32 phid;
    std::map<uint32, uint32> rewids;
};

class Recruitment
{
public:
	static Recruitment* instance()
	{
		static Recruitment instance;
		return &instance;
	}
    //招募
	RecruitmentRw * GetReParameters(uint8 level, uint8 checkone)
	{
		CRecruitmentRwContainer::iterator itr = m_RecruitmentRws.find(MAKE_PAIR16(level, checkone));
		if (itr != m_RecruitmentRws.end())
			return itr->second;
		return NULL;
	}

	RecruitmentRw const* GetReParameters(uint8 level, uint8 checkone) const
	{
		CRecruitmentRwContainer::const_iterator itr = m_RecruitmentRws.find(MAKE_PAIR16(level, checkone));
		if (itr != m_RecruitmentRws.end())
			return itr->second;
		return NULL;
	}
	typedef std::unordered_map<uint16, RecruitmentRw*> CRecruitmentRwContainer;
	CRecruitmentRwContainer m_RecruitmentRws;

    void Create(Player * player, std::string plname);
    void SendRewardMarkByMail(Player *plr, uint32 guid, uint32 rewId);
    void ForPlayerLevelup(Player * player, uint8 level);

    //伤害排行

    typedef std::vector<DmgPhrew*>  CDmgPhrewMap;
    CDmgPhrewMap m_DmgPhrewMaps; //光环叠加

    DmgPhrew * FindAuraOnDeathMod(uint32 phidnow)
    {
        for (auto i = m_DmgPhrewMaps.begin(); i != m_DmgPhrewMaps.end(); i++)
        {
            if ((*i)->phid == phidnow)
                return (*i);
        }
        return nullptr;
    }

    void loadAllChar();
    std::unordered_map<uint32, uint64> dmgmaps;
    std::vector<uint32> allmaps[12];
    uint32 GetClassDmgPH(Player * player);
    void ResetAllDmg(uint32 eventid);
    void SortAllDmg();
    void SortClassDmg(uint8 classid, uint32 guid, uint64 dmg);
    std::string GetClass(uint8 playerClass);
	void LoadWorld(); //加载世界奖励
    uint32 GetDmgRewID(uint32 phid, uint32 guidlow);
    void SendDmgPhByMail(uint32 guid, uint32 rewId, uint32 phid);
private:

};
#define sRecruitment Recruitment::instance()
