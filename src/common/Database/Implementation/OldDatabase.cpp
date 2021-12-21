/*
 * Copyright (C) 
 *
 * Copyright (C) 2008-2016 TrinityCore <http://www.trinitycore.org/>
 * Copyright (C) 2005-2009 MaNGOS <http://getmangos.com/>
 */

#include "OldDatabase.h"

void OldDatabaseConnection::DoPrepareStatements()
{
    if (!m_reconnecting)
		m_stmts.resize(MAX_OLDBASE_STATEMENTS);
}
