#include "access_token_handler.h"
#include "code.h"
#include "defines/agent_values.h"
#include "defines/ipc_values.h"
#include "device.h"
#include "list/list.h"
#include "password.h"
#include "refresh.h"
#include "utils/json.h"
#include "utils/listUtils.h"
#include "utils/logger.h"

/** @fn oidc_error_t tryRefreshFlow(struct oidc_account* p)
 * @brief tries to issue an access token for the specified account by using the
 * refresh flow
 * @param p a pointer to the account for whom an access token should be issued
 * @return 0 on success; 1 otherwise
 */
char* tryRefreshFlow(struct oidc_account* p, const char* scope,
                     struct ipcPipe pipes) {
  logger(DEBUG, "Trying Refresh Flow");
  if (!account_refreshTokenIsValid(p)) {
    logger(ERROR, "No refresh token found");
    oidc_errno = OIDC_ENOREFRSH;
    return NULL;
  }
  return refreshFlow(p, scope, pipes);
}

/** @fn oidc_error_t tryPasswordFlow(struct oidc_account* p)
 * @brief tries to issue an access token by using the password flow. The user
 * might be prompted for his username and password
 * @param p a pointer to the account for whom an access token should be issued
 * @return 0 on success; 1 otherwise
 */
oidc_error_t tryPasswordFlow(struct oidc_account* p, struct ipcPipe pipes) {
  logger(DEBUG, "Trying Password Flow");
  if (!strValid(account_getUsername(p)) || !strValid(account_getPassword(p))) {
    oidc_errno = OIDC_ECRED;
    logger(DEBUG, "No credentials found");
    return oidc_errno;
  }
  return passwordFlow(p, pipes);
}

/** @fn int tokenIsValidforSeconds(struct oidc_account p, time_t
 * min_valid_period)
 * @brief checks if the access token for an account is at least valid for the
 * given period of time
 * @param p the account whose access token should be checked
 * @param min_valid_period the period of time the access token should be valid
 * (at least)
 * @return 1 if the access_token is valid for the given time; 0 if not.
 */
int tokenIsValidForSeconds(const struct oidc_account* p,
                           time_t                     min_valid_period) {
  time_t now        = time(NULL);
  time_t expires_at = account_getTokenExpiresAt(p);
  return expires_at - now > 0 && expires_at - now > min_valid_period;
}

char* getAccessTokenUsingRefreshFlow(struct oidc_account* account,
                                     time_t min_valid_period, const char* scope,
                                     struct ipcPipe pipes) {
  if (scope == NULL && min_valid_period != FORCE_NEW_TOKEN &&
      strValid(account_getAccessToken(account)) &&
      tokenIsValidForSeconds(account, min_valid_period)) {
    return account_getAccessToken(account);
  }
  logger(DEBUG, "No acces token found that is valid long enough");
  return tryRefreshFlow(account, scope, pipes);
}

oidc_error_t getAccessTokenUsingPasswordFlow(struct oidc_account* account,
                                             struct ipcPipe       pipes) {
  if (strValid(account_getAccessToken(account))) {
    return OIDC_SUCCESS;
  }
  oidc_errno = tryPasswordFlow(account, pipes);
  return oidc_errno;
}

oidc_error_t getAccessTokenUsingAuthCodeFlow(struct oidc_account* account,
                                             const char*          code,
                                             const char*    used_redirect_uri,
                                             char*          code_verifier,
                                             struct ipcPipe pipes) {
  if (strValid(account_getAccessToken(account))) {
    return OIDC_SUCCESS;
  }
  oidc_errno =
      codeExchange(account, code, used_redirect_uri, code_verifier, pipes);
  return oidc_errno;
}

oidc_error_t getAccessTokenUsingDeviceFlow(struct oidc_account* account,
                                           const char*          device_code,
                                           struct ipcPipe       pipes) {
  if (strValid(account_getAccessToken(account))) {
    return OIDC_SUCCESS;
  }
  oidc_errno = lookUpDeviceCode(account, device_code, pipes);
  return oidc_errno;
}

struct flow_order {
  unsigned char refresh;
  unsigned char password;
  unsigned char code;
  unsigned char device;
};

list_t* parseFlow(const char* flow) {
  list_t* flows = list_new();
  flows->match  = (matchFunction)strequal;
  if (flow == NULL) {  // Using default order
    list_rpush(flows, list_node_new(FLOW_VALUE_REFRESH));
    list_rpush(flows, list_node_new(FLOW_VALUE_PASSWORD));
    list_rpush(flows, list_node_new(FLOW_VALUE_CODE));
    list_rpush(flows, list_node_new(FLOW_VALUE_DEVICE));
    return flows;
  }
  flows->free = (void (*)(void*)) & _secFree;
  if (flow[0] != '[') {
    list_rpush(flows, list_node_new(oidc_sprintf("%s", flow)));
    return flows;
  }
  list_destroy(flows);
  return JSONArrayStringToList(flow);
}
