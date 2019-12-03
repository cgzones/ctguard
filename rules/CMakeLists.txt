install (FILES
    00_format.xml
    00_generic.xml
    00_groups.xml
    00_interventions.xml
    09_grouping.xml
    10_test.xml
    50_dpkg.xml
    50_unix-chkpwd.xml
    DESTINATION /etc/ctguard/rules
    PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ
    )
