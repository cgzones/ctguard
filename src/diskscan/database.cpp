#include "database.hpp"

#include "../libs/sqlite/sqlitestatement.hpp"

namespace ctguard::diskscan {

database::database(const std::string & path) : m_db{ path, true }
{
    // TODO(cgzones): SELinux context

    ctguard::libs::sqlite::sqlite_statement create_stmt{ "CREATE TABLE IF NOT EXISTS `diskscan-data` ( "
                                                         "`name` VARCHAR(4096) NOT NULL UNIQUE PRIMARY KEY,"
                                                         "`user` VARCHAR(256) NOT NULL ,"
                                                         "`group` VARCHAR(256) NOT NULL ,"
                                                         "`size` BIGINT UNSIGNED NOT NULL ,"
                                                         "`inode` BIGINT UNSIGNED NOT NULL ,"
                                                         "`mode` INT UNSIGNED NOT NULL ,"
                                                         "`mtime` BIGINT UNSIGNED NOT NULL ,"
                                                         "`ctime` BIGINT UNSIGNED NOT NULL ,"
                                                         "`type` TINYINT UNSIGNED NOT NULL ,"
                                                         "`sha256` CHAR(65) NOT NULL ,"
                                                         "`target` VARCHAR(256) NOT NULL ,"
                                                         "`created` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ,"
                                                         "`updated` DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP ,"
                                                         "`scanned` BOOL NOT NULL DEFAULT TRUE"
                                                         " );",
                                                         m_db };
    create_stmt.run();

    // ctguard::libs::sqlite::sqlite_statement create_index{ "CREATE UNIQUE INDEX IF NOT EXISTS `name` ON `diskscan-data` ( `name` );", m_db };
    // create_index.run();
}

bool database::empty()
{
    ctguard::libs::sqlite::sqlite_statement create_stmt{ "SELECT COUNT(`name`) FROM `diskscan-data`;", m_db };

    auto result = create_stmt.run(true);

    auto it = result.begin();

    return ((*it).get<int>(0) == 0);
}

} /* namespace ctguard::diskscan */
