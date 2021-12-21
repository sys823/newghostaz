#include "DatabaseWorkerPool.h"
#include "MySQLConnection.h"

class OldDatabaseConnection : public MySQLConnection
{
    public:
        //- Constructors for sync and async connections
		OldDatabaseConnection(MySQLConnectionInfo& connInfo) : MySQLConnection(connInfo) { }
		OldDatabaseConnection(ACE_Activation_Queue* q, MySQLConnectionInfo& connInfo) : MySQLConnection(q, connInfo) { }

        //- Loads database type specific prepared statements
        void DoPrepareStatements();
};

typedef DatabaseWorkerPool<OldDatabaseConnection> OldDatabaseWorkerPool;

enum OldDatabaseStatements
{
    MAX_OLDBASE_STATEMENTS
};

