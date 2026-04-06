#ifndef CREDENTIALS_H
#define CREDENTIALS_H

// This string will be generated/populated by the controller installer.
// It allows the Agent to verify the Controller during the initial BOOT_TLS phase.
const char* CONTROLLER_ROOT_CA = 
    "-----BEGIN CERTIFICATE-----\n"
    "MIIB/TCCAWagAwIBAgIUe123... (Example Content) ...\n"
    "-----END CERTIFICATE-----";

#endif
