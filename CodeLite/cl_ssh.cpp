#include <wx/string.h>
#include <wx/translation.h>
#include "cl_ssh.h"

clSSH::clSSH(const wxString& host, const wxString& user, const wxString& pass, int port)
    : m_host(host)
    , m_username(user)
    , m_password(pass)
    , m_port(port)
    , m_session(NULL)
    , m_connected(false)
{
}

clSSH::~clSSH()
{
    Close();
}

void clSSH::Connect() throw(clException)
{
    m_session = ssh_new();
    if ( !m_session ) {
        throw clException("ssh_new failed!");
    }

    int verbosity = SSH_LOG_NOLOG;
    ssh_options_set(m_session, SSH_OPTIONS_HOST,          m_host.mb_str(wxConvUTF8).data());
    ssh_options_set(m_session, SSH_OPTIONS_LOG_VERBOSITY, &verbosity);
    ssh_options_set(m_session, SSH_OPTIONS_PORT,          &m_port);
    ssh_options_set(m_session, SSH_OPTIONS_USER,          GetUsername().mb_str().data());
    
    int rc = ssh_connect(m_session);
    if (rc != SSH_OK) {
        throw clException(ssh_get_error(m_session));
    }
    m_connected = true;
}

bool clSSH::AuthenticateServer(wxString &message) throw (clException)
{
    int state = -1, hlen = 0;
    unsigned char *hash = NULL;
    char *hexa = NULL;

    message.Clear();
    state = ssh_is_server_known(m_session);
    hlen = ssh_get_pubkey_hash(m_session, &hash);
    if (hlen < 0) {
        throw clException("Unable to obtain server public key!");
    }

    switch (state) {
    case SSH_SERVER_KNOWN_OK:
        free(hash);
        return true;

    case SSH_SERVER_KNOWN_CHANGED:
        hexa = ssh_get_hexa(hash, hlen);
        message << _("Host key for server changed: it is now:\n")
                << hexa << "\n"
                << _("Accept server authentication?");
        free(hexa);
        free(hash);
        return false;

    case SSH_SERVER_FOUND_OTHER:
        message << _("The host key for this server was not found but an other type of key exists.\n")
                << _("An attacker might change the default server key to confuse your client into thinking the key does not exist\n")
                << _("Accept server authentication?");
        free(hash);
        return false;

    case SSH_SERVER_FILE_NOT_FOUND:
        message << _("Could not find known host file.\n")
                << _("If you accept the host key here, the file will be automatically created.\n");
        /* fallback to SSH_SERVER_NOT_KNOWN behavior */
    case SSH_SERVER_NOT_KNOWN:
        hexa = ssh_get_hexa(hash, hlen);
        message << _("The server is unknown. Do you trust the host key?\n")
                << _("Public key hash: ") << hexa << "\n"
                << _("Accept server authentication?");
        free(hexa);
        free(hash);
        return false;

    default:
    case SSH_SERVER_ERROR:
        throw clException(wxString() << "An error occured: " << ssh_get_error(m_session));
    }
    return false;
}

void clSSH::AcceptServerAuthentication() throw (clException)
{
    if ( !m_session ) {
        throw clException("NULL SSH session");
    }
    ssh_write_knownhost(m_session);
}

void clSSH::Login() throw (clException)
{
    if ( !m_session ) {
        throw clException("NULL SSH session");
    }

    int rc;
    // rc = ssh_userauth_kbdint(m_session, NULL, NULL);
    // if ( false /*rc == SSH_AUTH_INFO*/ ) {
    //     while (rc == SSH_AUTH_INFO) {
    //         const char *name, *instruction;
    //         int nprompts, iprompt;
    //         name = ssh_userauth_kbdint_getname(m_session);
    //         instruction = ssh_userauth_kbdint_getinstruction(m_session);
    //         nprompts = ssh_userauth_kbdint_getnprompts(m_session);
    //         if (strlen(name) > 0)
    //             printf("%s\n", name);
    //         if (strlen(instruction) > 0)
    //             printf("%s\n", instruction);
    //         for (iprompt = 0; iprompt < nprompts; iprompt++) {
    //             const char *prompt;
    //             char echo;
    //             prompt = ssh_userauth_kbdint_getprompt(m_session, iprompt, &echo);
    //             if (echo) {
    //                 char buffer[128], *ptr;
    //                 printf("%s", prompt);
    //                 if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
    //                     throw clException(wxString() << "Login error: " << ssh_get_error(m_session));
    //                 }
    // 
    //                 buffer[sizeof(buffer) - 1] = '\0';
    //                 if ((ptr = strchr(buffer, '\n')) != NULL)
    //                     ptr = '\0';
    //                 if (ssh_userauth_kbdint_setanswer(m_session, iprompt, buffer) < 0) {
    //                     throw clException(wxString() << "Login error: " << ssh_get_error(m_session));
    // 
    //                 }
    //                 memset(buffer, 0, strlen(buffer));
    //             } else {
    //                 if (ssh_userauth_kbdint_setanswer(m_session, iprompt, GetPassword().mb_str(wxConvUTF8).data()) < 0) {
    //                     throw clException(wxString() << "Login error: " << ssh_get_error(m_session));
    //                 }
    //             }
    //         }
    //         rc = ssh_userauth_kbdint(m_session, NULL, NULL);
    //     }
    // 
    // } else {
        // interactive keyboard method failed, try another method
        rc = ssh_userauth_password(m_session, NULL, GetPassword().mb_str().data());
        if ( rc == SSH_AUTH_SUCCESS ) {
            return;
            
        } else if ( rc == SSH_AUTH_DENIED ) {
            throw clException("Login failed: invalid username/password");
            
        } else {
            throw clException(wxString() << _("Authentication error: ") << ssh_get_error(m_session));
        }
    // }
}

void clSSH::Close()
{
    if ( m_session && m_connected ) {
        ssh_disconnect(m_session);
    }
    
    if ( m_session ) {
        ssh_free(m_session);
    }
    
    m_connected = false;
    m_session = NULL;
}