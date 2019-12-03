install (FILES
    diskscan.conf
    intervention.conf
    logscan.conf
    research.conf
    DESTINATION /etc/ctguard
    PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ
    )

install (FILES
    block_ip.sh
    DESTINATION /etc/ctguard/interventions
    PERMISSIONS OWNER_WRITE OWNER_READ OWNER_EXECUTE GROUP_READ GROUP_EXECUTE
    )

install (FILES
    ctguard-diskscan.service
    ctguard-intervention.service
    ctguard-logscan.service
    ctguard-research.service
    DESTINATION /lib/systemd/system
    )
