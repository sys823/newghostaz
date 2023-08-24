struct RecruitmentRw
{
	uint8 level;
	int8 checkone;
	uint32 rewid;
	uint32 brewid;
};

class Recruitment
{
public:
	static Recruitment* instance()
	{
		static Recruitment instance;
		return &instance;
	}

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

	void LoadWorld(); //加载世界奖励
	void Create(Player * player, std::string plname);
	void SendRewardMarkByMail(Player *plr, uint32 guid, uint32 rewId);
	void ForPlayerLevelup(Player * player, uint8 level);
private:

};
#define sRecruitment Recruitment::instance()
