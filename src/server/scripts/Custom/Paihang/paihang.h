
struct paihangconf
{
    uint32 minci;
    uint32 jf_buff;
    uint32 kill_buff;
    uint32 on_buff;
    std::string jf_gonggao;
    std::string kill_gonggao;
    std::string on_gonggao;
};

class PaiHang
{
public:
	static PaiHang* instance()
	{
		static PaiHang instance;
		return &instance;
	}

    void Load();

    typedef std::map<uint32, paihangconf> paihangconf_t;
    paihangconf_t Vpaihangconf;
    const paihangconf * Findpaihang(uint32 entry)
    {
        paihangconf_t::const_iterator It = Vpaihangconf.find(entry);

        return (It != Vpaihangconf.end() ? &It->second : NULL);
    }

    void StartEvent(uint32 eventid);
    void SendPaiHangUpdate();
    void SendAndClearBufforGG(Player * pl, bool gongao = false);

    typedef std::map<uint32, std::string> CPAIHANG_jf;
    CPAIHANG_jf VCph_jf;
    typedef std::map<uint32, std::string> CPAIHANG_time;
    CPAIHANG_time VCph_time;
    typedef std::map<uint32, std::string> CPAIHANG_kill;
    CPAIHANG_kill VCph_kill;
    void AddPaiHangList(Player* player, Object* obj, uint32 typesid);
    uint32 maxpaihang;

    void LQJF(Player* player);
private:

};
#define sPaiHang PaiHang::instance()
