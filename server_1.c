#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <mysql/mysql.h>

#define SZREQUESTBUFFER 4194304
#define N_FDS_INIT 4096
#define SZSQLSTRING 10240

static char sqlString[SZSQLSTRING];
static char messageEsc[8192];
static char message[4096];
static char fileName[256];
static char userNameEsc[128];
static char toUserNameEsc[128];
static char fromUserNameEsc[128];
static char groupIdEsc[128];
static char passwordEsc[128];
static char userName[64];
static char toUserName[64];
static char fromUserName[64];
static char groupId[64];
static char password[64];
static char messageTime[32];

MYSQL mysql;

// #00
int handleLogout(int connectSocketFD)
{
    MYSQL_RES* sqlResult = NULL;
    MYSQL_ROW sqlRow;
    sprintf(sqlString, "SELECT UserName FROM UserInfo WHERE ConnectSocketFD = %d;", connectSocketFD);
    if (mysql_query(&mysql, sqlString))
    {
        printf("sql query error: %s\n", mysql_error(&mysql));
        goto errlbl;
    }
    sqlResult = mysql_store_result(&mysql);
    if (!sqlResult)
    {
        printf("sql result error: %s\n", mysql_error(&mysql));
        goto errlbl;
    }
    sqlRow = mysql_fetch_row(sqlResult);
    if (!sqlRow)
    {
        write(connectSocketFD, "#00|1", 6); // invalid connection
    }
    else
    {
        mysql_real_escape_string(&mysql, userNameEsc, sqlRow[0], strnlen(sqlRow[0], 63));
        sprintf(sqlString, "UPDATE UserInfo SET ConnectSocketFD = -1 WHERE ConnectSocketFD = %d;", connectSocketFD);
        if (mysql_query(&mysql, sqlString))
        {
            printf("sql query error: %s\n", mysql_error(&mysql));
            goto errlbl;
        }
        sprintf(sqlString, "UPDATE Friends SET IsOpen = 0 WHERE UserA = '%s';", userNameEsc);
        if (mysql_query(&mysql, sqlString))
        {
            printf("sql query error: %s\n", mysql_error(&mysql));
            goto errlbl;
        }
        sprintf(sqlString, "UPDATE InGroup SET IsOpen = 0 WHERE UserName = '%s';", userNameEsc);
        if (mysql_query(&mysql, sqlString))
        {
            printf("sql query error: %s\n", mysql_error(&mysql));
            goto errlbl;
        }
        write(connectSocketFD, "#00|0", 6); // successfully logged out
    }
    mysql_free_result(sqlResult);
    return -2;
errlbl:
    write(connectSocketFD, "#99|1", 6); // logout failed due to db error
    mysql_free_result(sqlResult);
    return -1;
}

// #01
// sign up // format: #01|UserName|Password
int qc_signUp(const char* req, int reqLen, int confd)
{
    MYSQL_RES* sqlResult = NULL;
    MYSQL_ROW sqlRow;
    sscanf(req + 4, "%63[^|]|%63[^\\0]", userName, password);
    mysql_real_escape_string(&mysql, userNameEsc, userName, strnlen(userName, 63));
    sprintf(sqlString, "SELECT UserName FROM UserInfo WHERE UserName = '%s';", userNameEsc);
    if (mysql_query(&mysql, sqlString))
    {
        printf("sql query error: %s\n", mysql_error(&mysql));
        goto errlbl;
    }
    sqlResult = mysql_store_result(&mysql);
    if (!sqlResult)
    {
        printf("sql result error: %s\n", mysql_error(&mysql));
        goto errlbl;
    }
    sqlRow = mysql_fetch_row(sqlResult);
    if (sqlRow)
    {
        write(confd, "#01|1", 6); // username already exists
    }
    else
    {
        mysql_real_escape_string(&mysql, passwordEsc, password, strnlen(password, 63));
        sprintf(sqlString, "INSERT INTO UserInfo (UserName, UserPassword) VALUES ('%s', '%s');", userNameEsc, passwordEsc);
        if (mysql_query(&mysql, sqlString))
        {
            printf("sql query error: %s\n", mysql_error(&mysql));
            goto errlbl;
        }
        write(confd, "#01|0", 6); // successfully registered
    }
    mysql_free_result(sqlResult);
    return 0;
errlbl:
    write(confd, "#99|1", 6); // sql database error
    mysql_free_result(sqlResult);
    return -1;
}

// #02
// login // format: #02|UserName|Password
int qc_login(const char* req, int reqLen, int confd)
{
    MYSQL_RES* sqlResult = NULL;
    MYSQL_ROW sqlRow;
    sscanf(req + 4, "%63[^|]|%63[^\\0]", userName, password);
    mysql_real_escape_string(&mysql, userNameEsc, userName, strnlen(userName, 63));
    sprintf(sqlString, "SELECT UserName FROM UserInfo WHERE UserName = '%s';", userNameEsc);
    if (mysql_query(&mysql, sqlString))
    {
        printf("sql query error: %s\n", mysql_error(&mysql));
        goto errlbl;
    }
    sqlResult = mysql_store_result(&mysql);
    if (!sqlResult)
    {
        printf("sql result error: %s\n", mysql_error(&mysql));
        goto errlbl;
    }
    sqlRow = mysql_fetch_row(sqlResult);
    if (!sqlRow)
    {
        write(confd, "#02|1", 6); // user does not exist
    }
    else
    {
        mysql_real_escape_string(&mysql, passwordEsc, password, strnlen(password, 63));
        sprintf(sqlString, "SELECT ConnectSocketFD, ImgNo FROM UserInfo WHERE UserName = '%s' AND UserPassword = '%s';", userNameEsc, passwordEsc);
        if (mysql_query(&mysql, sqlString))
        {
            printf("sql query error: %s\n", mysql_error(&mysql));
            goto errlbl;
        }
        mysql_free_result(sqlResult);
        sqlResult = mysql_store_result(&mysql);
        if (!sqlResult)
        {
            printf("sql result error: %s\n", mysql_error(&mysql));
            goto errlbl;
        }
        sqlRow = mysql_fetch_row(sqlResult);
        if (!sqlRow)
        {
            write(confd, "#02|2", 6); // password incorrect
        }
        else
        {
            int currentFD = atoi(sqlRow[0]), imgNo = atoi(sqlRow[1]);
            if (currentFD != -1)
            {
                write(currentFD, "#02|3", 6); // force currently logged in user to logout (must respond null req)
                write(confd, "#02|4", 6); // force currently user who is trying to login to retry
            }
            else
            {
                sprintf(sqlString, "UPDATE UserInfo SET ConnectSocketFD = %d WHERE UserName = '%s';", confd, userNameEsc);
                if (mysql_query(&mysql, sqlString))
                {
                    printf("sql query error: %s\n", mysql_error(&mysql));
                    goto errlbl;
                }
                sprintf(message, "#02|0|%d", imgNo);
                write(confd, message, strnlen(message, 4095) + 1); // login successful
            }
        }
    }
    mysql_free_result(sqlResult);
    return 0;
errlbl:
    write(confd, "#99|1", 6); // sql database error
    mysql_free_result(sqlResult);
    return -1;
}

// #03
// add a friend // format: #03|toUserName
int qc_addFriend(const char* req, int reqLen, int confd)
{
    MYSQL_RES* sqlResult = NULL;
    MYSQL_ROW sqlRow;
    strncpy(toUserName, req + 4, 64);
    sprintf(sqlString, "SELECT UserName from UserInfo WHERE ConnectSocketFD = %d;", confd);
    if (mysql_query(&mysql, sqlString))
    {
        printf("sql query error: %s\n", mysql_error(&mysql));
        goto errlbl;
    }
    sqlResult = mysql_store_result(&mysql);
    if (!sqlResult)
    {
        printf("sql result error: %s\n", mysql_error(&mysql));
        goto errlbl;
    }
    sqlRow = mysql_fetch_row(sqlResult);
    if (!sqlRow)
    {
        write(confd, "#03|1", 6); // invalid connection
    }
    else
    {
        // sqlRow[0] is now "fromUserName"
        mysql_real_escape_string(&mysql, fromUserNameEsc, sqlRow[0], strnlen(sqlRow[0], 63));
        mysql_real_escape_string(&mysql, toUserNameEsc, toUserName, strnlen(toUserName, 63));
        sprintf(sqlString, "SELECT UserName FROM UserInfo WHERE UserName = '%s';", toUserNameEsc);
        if (mysql_query(&mysql, sqlString))
        {
            printf("sql query error: %s\n", mysql_error(&mysql));
            goto errlbl;
        }
        mysql_free_result(sqlResult);
        sqlResult = mysql_store_result(&mysql);
        if (!sqlResult)
        {
            printf("sql result error: %s\n", mysql_error(&mysql));
            goto errlbl;
        }
        sqlRow = mysql_fetch_row(sqlResult);
        if (!sqlRow)
        {
            write(confd, "#03|2", 6); // user does not exist
        }
        else
        {
            sprintf(sqlString, "SELECT UserA from Friends WHERE UserA = '%s' AND UserB = '%s';", fromUserNameEsc, toUserNameEsc);
            if (mysql_query(&mysql, sqlString))
            {
                printf("sql query error: %s\n", mysql_error(&mysql));
                goto errlbl;
            }
            mysql_free_result(sqlResult);
            sqlResult = mysql_store_result(&mysql);
            if (!sqlResult)
            {
                printf("sql result error: %s\n", mysql_error(&mysql));
                goto errlbl;
            }
            sqlRow = mysql_fetch_row(sqlResult);
            if (!sqlRow) // A and B are not friends
            {
                sprintf(sqlString, "INSERT INTO Friends (UserA, UserB) VALUES ('%s', '%s');", fromUserNameEsc, toUserNameEsc);
                if (mysql_query(&mysql, sqlString))
                {
                    printf("sql query error: %s\n", mysql_error(&mysql));
                    goto errlbl;
                }
                sprintf(sqlString, "INSERT INTO Friends (UserA, UserB) VALUES ('%s', '%s');", toUserNameEsc, fromUserNameEsc);
                if (mysql_query(&mysql, sqlString))
                {
                    printf("sql query error: %s\n", mysql_error(&mysql));
                    goto errlbl;
                }
                write(confd, "#03|0", 6); // friend successfully added
            }
            else
            {
                write(confd, "#03|3", 6); // they are already friends
            }
        }
    }
    mysql_free_result(sqlResult);
    return 0;
errlbl:
    write(confd, "#99|1", 6); // sql database error
    mysql_free_result(sqlResult);
    return -1;
}

// #04
// send message // format: #04|ToUserName|messageTime|Message
int qc_sendMessage(const char* req, int reqLen, int confd)
{
    MYSQL_RES* sqlResult = NULL;
    MYSQL_ROW sqlRow;
    sscanf(req + 4, "%63[^|]|%31[^|]|%4095[^\\0]", toUserName, messageTime, message);
    sprintf(sqlString, "SELECT UserName FROM UserInfo WHERE ConnectSocketFD = %d;", confd);
    if (mysql_query(&mysql, sqlString))
    {
        printf("sql query error: %s\n", mysql_error(&mysql));
        goto errlbl;
    }
    sqlResult = mysql_store_result(&mysql);
    if (!sqlResult)
    {
        printf("sql result error: %s\n", mysql_error(&mysql));
        goto errlbl;
    }
    sqlRow = mysql_fetch_row(sqlResult);
    if (!sqlRow)
    {
        write(confd, "#04|1", 6); // invalid connection
    }
    else
    {
        strncpy(fromUserName, sqlRow[0], 64);
        mysql_real_escape_string(&mysql, toUserNameEsc, toUserName, strnlen(toUserName, 63));
        sprintf(sqlString, "SELECT ConnectSocketFD FROM UserInfo WHERE UserName = '%s';", toUserNameEsc);
        if (mysql_query(&mysql, sqlString))
        {
            printf("sql query error: %s\n", mysql_error(&mysql));
            goto errlbl;
        }
        mysql_free_result(sqlResult);
        sqlResult = mysql_store_result(&mysql);
        if (!sqlResult)
        {
            printf("sql result error: %s\n", mysql_error(&mysql));
            goto errlbl;
        }
        sqlRow = mysql_fetch_row(sqlResult);
        if (!sqlRow)
        {
            write(confd, "#04|2", 6); // user does not exist
        }
        else
        {
            int toUserFD = atoi(sqlRow[0]);
            mysql_real_escape_string(&mysql, fromUserNameEsc, fromUserName, strnlen(fromUserName, 63));
            sprintf(sqlString, "SELECT IsOpen FROM Friends WHERE UserA = '%s' AND UserB = '%s';", toUserNameEsc, fromUserNameEsc);
            if (mysql_query(&mysql, sqlString))
            {
                printf("sql query error: %s\n", mysql_error(&mysql));
                goto errlbl;
            }
            mysql_free_result(sqlResult);
            sqlResult = mysql_store_result(&mysql);
            if (!sqlResult)
            {
                printf("sql result error: %s\n", mysql_error(&mysql));
                goto errlbl;
            }
            sqlRow = mysql_fetch_row(sqlResult);
            if (!sqlRow)
            {
                write(confd, "#04|3", 6); // not friend, can't send message
            }
            else
            {
                if (toUserFD != -1 && atoi(sqlRow[0]))
                {
                    sprintf(messageEsc, "#04|4|%s|%s|%s", fromUserName, messageTime, message);
                    write(toUserFD, messageEsc, strnlen(messageEsc, 4095) + 1);
                    write(confd, "#04|0", 6); // message successfully sent // TODO feed back?
                }
                else
                {
                    mysql_real_escape_string(&mysql, messageEsc, message, strnlen(message, 4095));
                    sprintf(sqlString, "INSERT INTO MessageTable (MessageTime, FromUserName, ToUserName, Message) VALUES ('%s', '%s', '%s', '%s');", messageTime, fromUserNameEsc, toUserNameEsc, messageEsc);
                    if (mysql_query(&mysql, sqlString))
                    {
                        printf("sql query error: %s\n", mysql_error(&mysql));
                        goto errlbl;
                    }
                    write(confd, "#04|0", 6); // message stored to db
                }
            }
        }
    }
    mysql_free_result(sqlResult);
    return 0;
errlbl:
    write(confd, "#99|1", 6); // sql database error
    mysql_free_result(sqlResult);
    return -1;
}

// #05
// open chat box // format: #05|whoToChat
// client->server: #05|who_to_chat
// server->client: #05|0|messageTime|message (or #05|1 for invalid connection or #05|2 for end of transmission)
// client->server: #05|who_to_chat
// ......and so on
// MessageTable (MessageTime, FromUserName, ToUserName, Message)
int qc_openChatBox(const char* req, int reqLen, int confd)
{
    MYSQL_RES* sqlResult = NULL;
    MYSQL_ROW sqlRow;
    strncpy(fromUserName, req + 4, 64); //userName:FromUserName
    mysql_real_escape_string(&mysql, fromUserNameEsc, fromUserName, strnlen(fromUserName, 63));
    sprintf(sqlString, "SELECT UserName FROM UserInfo WHERE ConnectSocketFD = %d;", confd);
    if (mysql_query(&mysql, sqlString))
    {
        printf("sql query error: %s\n", mysql_error(&mysql));
        goto errlbl;
    }
    sqlResult = mysql_store_result(&mysql);
    if (!sqlResult)
    {
        printf("sql result error: %s\n", mysql_error(&mysql));
        goto errlbl;
    }
    sqlRow = mysql_fetch_row(sqlResult);
    if (!sqlRow)
    {
        write(confd, "#05|1", 6); // invalid connection
    }
    else
    {
        mysql_real_escape_string(&mysql, toUserNameEsc, sqlRow[0], strnlen(sqlRow[0], 63));
        sprintf(sqlString, "SELECT MessageTime, Message FROM MessageTable WHERE FromUserName =  '%s' AND ToUserName = '%s' ORDER BY MessageTime ASC;", fromUserNameEsc, toUserNameEsc);
        if (mysql_query(&mysql, sqlString))
        {
            printf("sql query error: %s\n", mysql_error(&mysql));
            goto errlbl;
        }
        mysql_free_result(sqlResult);
        sqlResult = mysql_store_result(&mysql);
        if (!sqlResult)
        {
            printf("sql result error: %s\n", mysql_error(&mysql));
            goto errlbl;
        }
        if ((sqlRow = mysql_fetch_row(sqlResult)))
        {
            sprintf(message, "#05|0|%s|%s", sqlRow[0], sqlRow[1]); // #05|0|MessageTime|Message
            write(confd, message, strnlen(message, 4095) + 1);
            sprintf(sqlString, "DELETE FROM MessageTable WHERE MessageTime = '%s' AND FromUserName = '%s' AND ToUserName = '%s';", sqlRow[0], fromUserNameEsc, toUserNameEsc);
            if (mysql_query(&mysql, sqlString))
            {
                printf("sql query error: %s\n", mysql_error(&mysql));
                goto errlbl;
            }
        }
        else
        {
            sprintf(sqlString, "UPDATE Friends SET IsOpen = 1 WHERE UserA = '%s' AND UserB = '%s';", toUserNameEsc, fromUserNameEsc);
            if (mysql_query(&mysql, sqlString))
            {
                printf("sql query error: %s\n", mysql_error(&mysql));
                goto errlbl;
            }
            write(confd, "#05|2", 6); // end of transmission
        }
    }
    mysql_free_result(sqlResult);
    return 0;
errlbl:
    write(confd, "#99|1", 6); // sql database error
    mysql_free_result(sqlResult);
    return -1;
}

// #06
// request friend info
// client->server:#06|0
// (db stores friend information into a temp table)
// server->client:#06|0|x|friendName|imgNo|nMessages (or #06|1 for invalid connection) ps: (x==1,friend is online,vise versa) n stands for n messages
// client->server:#06|1 (for next piece of friend info)
// server->client:#06|0|x|friendName|imgNo|nMessages (next piece of info)
// client->server:#06|1 (for next piece of friend info)
// ............
// server->client:#06|2 (no more info)
int qc_requestFriendInfo(const char* req, int reqLen, int confd)
{
    MYSQL_RES* sqlResult = NULL;
    MYSQL_ROW sqlRow;
    sprintf(sqlString, "SELECT UserName FROM UserInfo WHERE ConnectSocketFD = %d;", confd);
    if (mysql_query(&mysql, sqlString))
    {
        printf("sql query error: %s\n", mysql_error(&mysql));
        goto errlbl;
    }
    sqlResult = mysql_store_result(&mysql);
    if (!sqlResult)
    {
        printf("sql result error: %s\n", mysql_error(&mysql));
        goto errlbl;
    }
    sqlRow = mysql_fetch_row(sqlResult);
    if (!sqlRow)
    {
        write(confd, "#06|1", 6); // invalid connection
    }
    else
    {
        strncpy(toUserName, sqlRow[0], 64);
        mysql_real_escape_string(&mysql, toUserNameEsc, toUserName, strnlen(toUserName, 63));
        if (!(req[4] & 0x0f))
        {
            sprintf(sqlString, "DROP TEMPORARY TABLE IF EXISTS TEMP_%d;", confd);
            if (mysql_query(&mysql, sqlString))
            {
                printf("sql query error: %s\n", mysql_error(&mysql));
                goto errlbl;
            }
            sprintf(sqlString, "CREATE TEMPORARY TABLE TEMP_%d (SELECT ConnectSocketFD, UserB, ImgNo FROM Friends, UserInfo WHERE Friends.UserA = '%s' AND Friends.UserB = UserInfo.UserName);", confd, toUserNameEsc);
            if (mysql_query(&mysql, sqlString))
            {
                printf("sql query error: %s\n", mysql_error(&mysql));
                goto errlbl;
            }
        }
        sprintf(sqlString, "CREATE TEMPORARY TABLE IF NOT EXISTS TEMP_%d (ConnectSocketFD INT DEFAULT -1, UserB VARCHAR(64) NOT NULL, ImgNo INT DEFAULT 0);", confd);
        if (mysql_query(&mysql, sqlString))
        {
            printf("sql query error: %s\n", mysql_error(&mysql));
            goto errlbl;
        }
        sprintf(sqlString, "SELECT ConnectSocketFD, UserB, ImgNo FROM TEMP_%d;", confd);
        if (mysql_query(&mysql, sqlString))
        {
            printf("sql query error: %s\n", mysql_error(&mysql));
            goto errlbl;
        }
        mysql_free_result(sqlResult);
        sqlResult = mysql_store_result(&mysql);
        if (!sqlResult)
        {
            printf("sql result error: %s\n", mysql_error(&mysql));
            goto errlbl;
        }
        sqlRow = mysql_fetch_row(sqlResult);
        if (!sqlRow)
        {
            sprintf(sqlString, "DROP TEMPORARY TABLE TEMP_%d;", confd);
            if (mysql_query(&mysql, sqlString))
            {
                printf("sql query error: %s\n", mysql_error(&mysql));
                goto errlbl;
            }
            write(confd, "#06|2", 6); // no more info
        }
        else
        {
            int fromUserFD = atoi(sqlRow[0]), imgNo = atoi(sqlRow[2]);
            strncpy(fromUserName, sqlRow[1], 64);
            mysql_real_escape_string(&mysql, fromUserNameEsc, fromUserName, strnlen(fromUserName, 63));
            sprintf(sqlString, "DELETE FROM TEMP_%d WHERE UserB = '%s';", confd, fromUserNameEsc);
            if (mysql_query(&mysql, sqlString))
            {
                printf("sql query error: %s\n", mysql_error(&mysql));
                goto errlbl;
            }
            sprintf(sqlString, "SELECT COUNT(*) FROM MessageTable WHERE FromUserName = '%s' AND ToUserName = '%s';", fromUserNameEsc, toUserNameEsc);
            if (mysql_query(&mysql, sqlString))
            {
                printf("sql query error: %s\n", mysql_error(&mysql));
                goto errlbl;
            }
            mysql_free_result(sqlResult);
            sqlResult = mysql_store_result(&mysql);
            if (!sqlResult)
            {
                printf("sql result error: %s\n", mysql_error(&mysql));
                goto errlbl;
            }
            sqlRow = mysql_fetch_row(sqlResult);
            int nMessages = atoi(sqlRow[0]);
            sprintf(message, "#06|0|%c|%s|%d|%d", fromUserFD != -1 ? '1' : '0', fromUserName, imgNo, nMessages);
            write(confd, message, strnlen(message, 4095) + 1);
        }
    }
    mysql_free_result(sqlResult);
    return 0;
errlbl:
    write(confd, "#99|1", 6); // sql database error
    mysql_free_result(sqlResult);
    return -1;
}

// #07
// delete friend // format: #07|toUserName 
int qc_deleteFriend(const char* req, int reqLen, int confd)
{
    MYSQL_RES* sqlResult = NULL;
    MYSQL_ROW sqlRow;
    sprintf(sqlString, "SELECT UserName FROM UserInfo WHERE ConnectSocketFD = %d;", confd);
    if (mysql_query(&mysql, sqlString))
    {
        printf("sql query error: %s\n", mysql_error(&mysql));
        goto errlbl;
    }
    sqlResult = mysql_store_result(&mysql);
    if (!sqlResult)
    {
        printf("sql result error: %s\n", mysql_error(&mysql));
        goto errlbl;
    }
    sqlRow = mysql_fetch_row(sqlResult);
    if (!sqlRow)
    {
        write(confd, "#07|1", 6); // invalid connection
    }
    else
    {
        strncpy(toUserName, req + 4, 64);
        mysql_real_escape_string(&mysql, fromUserNameEsc, sqlRow[0], strnlen(sqlRow[0], 63));
        mysql_real_escape_string(&mysql, toUserNameEsc, toUserName, strnlen(toUserName, 63));
        sprintf(sqlString, "DELETE FROM Friends WHERE UserA = '%s' AND UserB = '%s';", toUserNameEsc, fromUserNameEsc);
        if (mysql_query(&mysql, sqlString))
        {
            printf("sql query error: %s\n", mysql_error(&mysql));
            goto errlbl;
        }
        sprintf(sqlString, "DELETE FROM Friends WHERE UserA = '%s' AND UserB = '%s';", fromUserNameEsc, toUserNameEsc);
        if (mysql_query(&mysql, sqlString))
        {
            printf("sql query error: %s\n", mysql_error(&mysql));
            goto errlbl;
        }
        write(confd, "#07|0", 6); // successfully deleted
    }
    mysql_free_result(sqlResult);
    return 0;
errlbl:
    write(confd, "#99|1", 6); // sql database error
    mysql_free_result(sqlResult);
    return -1;
}

// #08
// delete account // format: #08
int qc_calcelAccount(int confd)
{
    MYSQL_RES* sqlResult = NULL;
    MYSQL_ROW sqlRow;
    sprintf(sqlString, "SELECT UserName FROM UserInfo WHERE ConnectSocketFD = %d;", confd);
    if (mysql_query(&mysql, sqlString))
    {
        printf("sql query error: %s\n", mysql_error(&mysql));
        goto errlbl;
    }
    sqlResult = mysql_store_result(&mysql);
    if (!sqlResult)
    {
        printf("sql result error: %s\n", mysql_error(&mysql));
        goto errlbl;
    }
    sqlRow = mysql_fetch_row(sqlResult);
    if (!sqlRow)
    {
        write(confd, "#08|1", 6); // invalid connection
    }
    else
    {
        strncpy(userName, sqlRow[0], 64);
        mysql_real_escape_string(&mysql, userNameEsc, userName, strnlen(userName, 63));
        sprintf(sqlString, "DELETE FROM UserInfo WHERE UserName = '%s';", userNameEsc);
        if (mysql_query(&mysql, sqlString))
        {
            printf("sql query error: %s\n", mysql_error(&mysql));
            goto errlbl;
        }
        sprintf(sqlString, "DELETE FROM Friends WHERE UserA = '%s' OR UserB = '%s';", userNameEsc, userNameEsc);
        if (mysql_query(&mysql, sqlString))
        {
            printf("sql query error: %s\n", mysql_error(&mysql));
            goto errlbl;
        }
        sprintf(sqlString, "DELETE FROM MessageTable WHERE FromUserName = '%s' OR ToUserName = '%s';", userNameEsc, userNameEsc);
        if (mysql_query(&mysql, sqlString))
        {
            printf("sql query error: %s\n", mysql_error(&mysql));
            goto errlbl;
        }
        sprintf(sqlString, "DELETE FROM GroupInfo WHERE GroupOwner = '%s';", userNameEsc);
        if (mysql_query(&mysql, sqlString))
        {
            printf("sql query error: %s\n", mysql_error(&mysql));
            goto errlbl;
        }
        sprintf(sqlString, "DELETE FROM GroupMessageTable WHERE FromUserName = '%s' OR ToUserName = '%s';", userNameEsc, userNameEsc);
        if (mysql_query(&mysql, sqlString))
        {
            printf("sql query error: %s\n", mysql_error(&mysql));
            goto errlbl;
        }
        sprintf(sqlString, "DELETE FROM InGroup WHERE UserName = '%s';", userNameEsc);
        if (mysql_query(&mysql, sqlString))
        {
            printf("sql query error: %s\n", mysql_error(&mysql));
            goto errlbl;
        }
        write(confd, "#08|0", 6); // account successfully deleted
    }
    mysql_free_result(sqlResult);
    return -2;
errlbl:
    write(confd, "#99|1", 6); // sql database error
    mysql_free_result(sqlResult);
    return -1;
}

// #09
// set head image // format: #09|image number
int qc_setHeadImage(const char* req, int reqLen, int confd)
{
    MYSQL_RES* sqlResult = NULL;
    MYSQL_ROW sqlRow;
    int imgNo = atoi(req + 4);
    sprintf(sqlString, "SELECT UserName FROM UserInfo WHERE ConnectSocketFD = %d;", confd);
    if (mysql_query(&mysql, sqlString))
    {
        printf("sql query error: %s\n", mysql_error(&mysql));
        goto errlbl;
    }
    sqlResult = mysql_store_result(&mysql);
    if (!sqlResult)
    {
        printf("sql result error: %s\n", mysql_error(&mysql));
        goto errlbl;
    }
    sqlRow = mysql_fetch_row(sqlResult);
    if (!sqlRow)
    {
        write(confd, "#09|1", 6); // invalid connection
    }
    else
    {
        mysql_real_escape_string(&mysql, userNameEsc, sqlRow[0], strnlen(sqlRow[0], 63));
        sprintf(sqlString, "UPDATE UserInfo SET ImgNo = %d WHERE UserName = '%s';", imgNo, userNameEsc);
        if (mysql_query(&mysql, sqlString))
        {
            printf("sql query error: %s\n", mysql_error(&mysql));
            goto errlbl;
        }
        write(confd, "#09|0", 6); // head image set successfully
    }
    mysql_free_result(sqlResult);
    return 0;
errlbl:
    write(confd, "#99|1", 6); // sql database error
    mysql_free_result(sqlResult);
    return -1;
}

// #10
// close chat box
// client->server   #10|whoToClose
// server->client   #10|0    success
int qc_closeChatBox(const char* req, int reqLen, int confd)
{
    MYSQL_RES* sqlResult = NULL;
    MYSQL_ROW sqlRow;
    strncpy(fromUserName, req + 4, 64); // fromUserName: 被关闭者
    mysql_real_escape_string(&mysql, fromUserNameEsc, fromUserName, strnlen(fromUserName, 63));
    sprintf(sqlString, "SELECT UserName FROM UserInfo WHERE ConnectSocketFD = %d;", confd);
    if (mysql_query(&mysql, sqlString))
    {
        printf("sql query error: %s\n", mysql_error(&mysql));
        goto errlbl;
    }
    sqlResult = mysql_store_result(&mysql);
    if (!sqlResult)
    {
        printf("sql result error: %s\n", mysql_error(&mysql));
        goto errlbl;
    }
    sqlRow = mysql_fetch_row(sqlResult);
    if (!sqlRow)
    {
        write(confd, "#10|1", 6); // invalid connection
    }
    else
    {
        mysql_real_escape_string(&mysql, toUserNameEsc, sqlRow[0], strnlen(sqlRow[0], 63)); // toUserName 关闭者
        sprintf(sqlString, "UPDATE Friends SET IsOpen = 0 WHERE UserA = '%s' AND UserB = '%s';", toUserNameEsc, fromUserNameEsc);
        if (mysql_query(&mysql, sqlString))
        {
            printf("sql query error: %s\n", mysql_error(&mysql));
            goto errlbl;
        }
        write(confd, "#10|0", 6); // chat box successfully closed
    }
    mysql_free_result(sqlResult);
    return 0;
errlbl:
    write(confd, "#99|1", 6); // sql database error
    mysql_free_result(sqlResult);
    return -1;
}

// #11
// send file

// 1.sender->server: #11|0|receiverUserName|messageTime|fileName
// 2.server->receiver: #11|5|senderUserName|messageTime|fileName
// 3.receiver->server: #11|2|senderUserName (fileName received)
// 4.server->sender: #11|0|receiverUserName (success)
// 5.sender->server: #11|1|receiverUserName(64)|messageTime(20)|nCurrent(11)|nTotal(11)|nValid(11)|bytesInFile(1-4194176)
//                       ^ ^                    ^               ^            ^          ^          ^
//                       | |                    |               |            |          |          |
//                       4 6                    71              92          104        116        128
// 6.server->receiver: #11|6|senderUserName(64)|messageTime(20)|nCurrent(11)|nTotal(11)|nValid(11)|bytesInFile(1-4194176)
//                         ^ ^                  ^               ^            ^          ^          ^
//                         | |                  |               |            |          |          |
//                         4 6                  71              92          104        116        128
// 7.receiver->server: #11|2|senderUserName (fileContent received)
// 8.server->sender: #11|0|receiverUserName (success)                    (goto 5. until the file is successfully sent)

// nCurrent: current piece number
// nTotal: total number of the pieces
// nValid: valid bytes in the file from req[128] on (1 <= nValid <= 4194176) (4194176 == 2^22 - 2^7)

// errors:
// server->client (sender or receiver):
// #11|0                  success
// #11|1                  invalid connection
// #11|2                  user does not exist
// #11|3                  not friend, can't send file
// #11|4                  friend not online
int qc_sendFile(char* req, int reqLen, int confd)
{
    MYSQL_RES* sqlResult = NULL;
    MYSQL_ROW sqlRow;
    int reqType = req[4] & 0x0f;
    sprintf(sqlString, "SELECT UserName FROM UserInfo WHERE ConnectSocketFD = %d;", confd);
    if (mysql_query(&mysql, sqlString))
    {
        printf("sql query error: %s\n", mysql_error(&mysql));
        goto errlbl;
    }
    sqlResult = mysql_store_result(&mysql);
    if (!sqlResult)
    {
        printf("sql result error: %s\n", mysql_error(&mysql));
        goto errlbl;
    }
    sqlRow = mysql_fetch_row(sqlResult);
    if (!sqlRow)
    {
        write(confd, "#11|1", 6); // invalid connection
    }
    else
    {
        strncpy(fromUserName, sqlRow[0], 64);
        sscanf(req + 6, "%63[^|\\0]", toUserName);
        mysql_real_escape_string(&mysql, toUserNameEsc, toUserName, strnlen(toUserName, 63));
        sprintf(sqlString, "SELECT ConnectSocketFD FROM UserInfo WHERE UserName = '%s';", toUserNameEsc);
        if (mysql_query(&mysql, sqlString))
        {
            printf("sql query error: %s\n", mysql_error(&mysql));
            goto errlbl;
        }
        mysql_free_result(sqlResult);
        sqlResult = mysql_store_result(&mysql);
        if (!sqlResult)
        {
            printf("sql result error: %s\n", mysql_error(&mysql));
            goto errlbl;
        }
        sqlRow = mysql_fetch_row(sqlResult);
        if (!sqlRow)
        {
            write(confd, "#11|2", 6); // user does not exist
        }
        else
        {
            int toUserFD = atoi(sqlRow[0]);
            mysql_real_escape_string(&mysql, fromUserNameEsc, fromUserName, strnlen(fromUserName, 63));
            sprintf(sqlString, "SELECT IsOpen FROM Friends WHERE UserA = '%s' AND UserB = '%s';", toUserNameEsc, fromUserNameEsc);
            if (mysql_query(&mysql, sqlString))
            {
                printf("sql query error: %s\n", mysql_error(&mysql));
                goto errlbl;
            }
            mysql_free_result(sqlResult);
            sqlResult = mysql_store_result(&mysql);
            if (!sqlResult)
            {
                printf("sql result error: %s\n", mysql_error(&mysql));
                goto errlbl;
            }
            sqlRow = mysql_fetch_row(sqlResult);
            if (!sqlRow)
            {
                write(confd, "#11|3", 6); // not friend, can't send file
            }
            else
            {
                if (toUserFD != -1) // friend is online
                {
                    if (reqType == 2) // send feedback
                    {
                        sprintf(message, "#11|0|%s", fromUserName);
                        write(toUserFD, message, strnlen(message, 4095) + 1); // success
                    }
                    else if (reqType == 1) // send file content
                    {
                        req[4] = '6';
                        strncpy(req + 6, fromUserName, 64);
                        write(toUserFD, req, atol(req + 116) + 128);
                    }
                    else // send file name
                    {
                        sscanf(req, "#11|0|%63[^|]|%31[^|]|%255[^\\0]", toUserName, messageTime, fileName);
                        sprintf(message, "#11|5|%s|%s|%s", fromUserName, messageTime, fileName);
                        write(toUserFD, message, strnlen(message, 4095) + 1);
                    }
                }
                else
                {
                    write(confd, "#11|4", 6); // friend is not online
                }
            }
        }
    }
    mysql_free_result(sqlResult);
    return 0;
errlbl:
    write(confd, "#99|1", 6); // sql database error
    mysql_free_result(sqlResult);
    return -1;
}

// #12 create group
// client->server #12|groupId
// server->client #12|1 invalid connection
//                #12|2 groupId has been used
//                #12|0 group is created successfully
int qc_createGroup(char* req, int reqLen, int confd)
{
    MYSQL_RES* sqlResult = NULL;
    MYSQL_ROW sqlRow;
    strncpy(groupId, req + 4, 64);
    sprintf(sqlString, "SELECT UserName FROM UserInfo WHERE ConnectSocketFD = %d;", confd);
    if (mysql_query(&mysql, sqlString))
    {
        printf("sql query error: %s\n", mysql_error(&mysql));
        goto errlbl;
    }
    sqlResult = mysql_store_result(&mysql);
    if (!sqlResult)
    {
        printf("sql result error: %s\n", mysql_error(&mysql));
        goto errlbl;
    }
    sqlRow = mysql_fetch_row(sqlResult);
    if (!sqlRow)
    {
        write(confd, "#12|1", 6); // invalid connection
    }
    else
    {
        strncpy(userName, sqlRow[0], 64);
        mysql_real_escape_string(&mysql, groupIdEsc, groupId, strnlen(groupId, 63));
        sprintf(sqlString, "SELECT GroupId FROM GroupInfo WHERE GroupId = '%s';", groupIdEsc);
        if (mysql_query(&mysql, sqlString))
        {
            printf("sql query error: %s\n", mysql_error(&mysql));
            goto errlbl;
        }
        mysql_free_result(sqlResult);
        sqlResult = mysql_store_result(&mysql);
        if (!sqlResult)
        {
            printf("sql result error: %s\n", mysql_error(&mysql));
            goto errlbl;
        }
        sqlRow = mysql_fetch_row(sqlResult);
        if (sqlRow)
        {
            write(confd, "#12|2", 6); // groupId has been used
        }
        else
        {
            mysql_real_escape_string(&mysql, userNameEsc, userName, strnlen(userName, 63));
            sprintf(sqlString, "INSERT INTO GroupInfo (GroupId, GroupOwner) VALUES ('%s', '%s');", groupIdEsc, userNameEsc);
            if (mysql_query(&mysql, sqlString))
            {
                printf("sql query error: %s\n", mysql_error(&mysql));
                goto errlbl;
            }
            sprintf(sqlString, "INSERT INTO InGroup (GroupId, UserName) VALUES ('%s', '%s');", groupIdEsc, userNameEsc);
            if (mysql_query(&mysql, sqlString))
            {
                printf("sql query error: %s\n", mysql_error(&mysql));
                goto errlbl;
            }
            write(confd, "#12|0", 6); // group has been created successfully
        }
    }
    mysql_free_result(sqlResult);
    return 0;
errlbl:
    write(confd, "#99|1", 6); // sql database error
    mysql_free_result(sqlResult);
    return -1;
}

// #13 apply into group
// client->server: #13|groupId
// server->client: #13|1 invalid connection
//                 #13|2 群聊不存在
//                 #13|3 已经在群内
//                 #13|0 申请成功
int qc_applyToGroup(char* req, int reqLen, int confd)
{
    MYSQL_RES* sqlResult = NULL;
    MYSQL_ROW sqlRow;
    strncpy(groupId, req + 4, 64);
    sprintf(sqlString, "SELECT UserName FROM UserInfo WHERE ConnectSocketFD = %d;", confd);
    if (mysql_query(&mysql, sqlString))
    {
        printf("sql query error: %s\n", mysql_error(&mysql));
        goto errlbl;
    }
    sqlResult = mysql_store_result(&mysql);
    if (!sqlResult)
    {
        printf("sql result error: %s\n", mysql_error(&mysql));
        goto errlbl;
    }
    sqlRow = mysql_fetch_row(sqlResult);
    if (!sqlRow)
    {
        write(confd, "#13|1", 6); // invalid connection
    }
    else
    {
        strncpy(userName, sqlRow[0], 64);
        mysql_real_escape_string(&mysql, groupIdEsc, groupId, strnlen(groupId, 63));
        sprintf(sqlString, "SELECT GroupId FROM GroupInfo WHERE GroupId = '%s';", groupIdEsc);
        if (mysql_query(&mysql, sqlString))
        {
            printf("sql query error: %s\n", mysql_error(&mysql));
            goto errlbl;
        }
        mysql_free_result(sqlResult);
        sqlResult = mysql_store_result(&mysql);
        if (!sqlResult)
        {
            printf("sql result error: %s\n", mysql_error(&mysql));
            goto errlbl;
        }
        sqlRow = mysql_fetch_row(sqlResult);
        if (!sqlRow)
        {
            write(confd, "#13|2", 6); // group does not exist
        }
        else
        {
            mysql_real_escape_string(&mysql, userNameEsc, userName, strnlen(userName, 63));
            sprintf(sqlString, "SELECT GroupId, UserName FROM InGroup WHERE GroupId = '%s' AND UserName = '%s';", groupIdEsc, userNameEsc);
            if (mysql_query(&mysql, sqlString))
            {
                printf("sql query error: %s\n", mysql_error(&mysql));
                goto errlbl;
            }
            mysql_free_result(sqlResult);
            sqlResult = mysql_store_result(&mysql);
            if (!sqlResult)
            {
                printf("sql result error: %s\n", mysql_error(&mysql));
                goto errlbl;
            }
            sqlRow = mysql_fetch_row(sqlResult);
            if (sqlRow)
            {
                write(confd, "#13|3", 6); // already in group
            }
            else
            {
                sprintf(sqlString, "INSERT INTO InGroup (GroupId, UserName) VALUES ('%s','%s');", groupIdEsc, userNameEsc);
                if (mysql_query(&mysql, sqlString))
                {
                    printf("sql query error: %s\n", mysql_error(&mysql));
                    goto errlbl;
                }
                write(confd, "#13|0", 6); // success
            }
        }
    }
    mysql_free_result(sqlResult);
    return 0;
errlbl:
    write(confd, "#99|1", 6); // sql database error
    mysql_free_result(sqlResult);
    return -1;
}

// #14
// send message in group
// client1->server: #14|groupId|messageTime|Message
// server->client1:
//                  #14|0   发送成功
//                  #14|1   非法连接
// server->client2:
//                  #14|2|groupId|FromWhom|Time|Message
int qc_sendMessageInGroup(const char* req, int reqLen, int confd)
{
    MYSQL_RES* sqlResult = NULL;
    MYSQL_ROW sqlRow;
    MYSQL_RES* sqlResult1 = NULL;
    MYSQL_ROW sqlRow1;
    sscanf(req + 4, "%63[^|]|%31[^|]|%4095[^\\0]", groupId, messageTime, message);
    mysql_real_escape_string(&mysql, groupIdEsc, groupId, strnlen(groupId, 63));
    sprintf(sqlString, "SELECT UserName FROM UserInfo WHERE ConnectSocketFD = %d;", confd);
    if (mysql_query(&mysql, sqlString))
    {
        printf("sql query error: %s\n", mysql_error(&mysql));
        goto errlbl;
    }
    sqlResult = mysql_store_result(&mysql);
    if (!sqlResult)
    {
        printf("sql result error: %s\n", mysql_error(&mysql));
        goto errlbl;
    }
    sqlRow = mysql_fetch_row(sqlResult);
    if (!sqlRow)
    {
        write(confd, "#14|1", 6); // invalid connection
    }
    else
    {
        strncpy(fromUserName, sqlRow[0], 64);
        mysql_real_escape_string(&mysql, fromUserNameEsc, fromUserName, strnlen(fromUserName, 63));
        sprintf(sqlString, "SELECT UserName FROM InGroup WHERE GroupId = '%s' AND UserName <> '%s';", groupIdEsc, fromUserNameEsc);
        if (mysql_query(&mysql, sqlString))
        {
            printf("sql query error: %s\n", mysql_error(&mysql));
            goto errlbl;
        }
        mysql_free_result(sqlResult);
        sqlResult = mysql_store_result(&mysql);
        if (!sqlResult)
        {
            printf("sql result error: %s\n", mysql_error(&mysql));
            goto errlbl;
        }
        while ((sqlRow = mysql_fetch_row(sqlResult))) // User in group
        {
            mysql_real_escape_string(&mysql, toUserNameEsc, sqlRow[0], strnlen(sqlRow[0], 63));
            sprintf(sqlString, "SELECT ConnectSocketFD, IsOpen FROM UserInfo, InGroup WHERE UserInfo.UserName = '%s' AND InGroup.UserName = '%s' AND InGroup.GroupId = '%s';", toUserNameEsc, toUserNameEsc, groupIdEsc);
            if (mysql_query(&mysql, sqlString))
            {
                printf("sql query error: %s\n", mysql_error(&mysql));
                goto errlbl;
            }
            mysql_free_result(sqlResult1);
            sqlResult1 = mysql_store_result(&mysql);
            if (!sqlResult1)
            {
                printf("sql result error: %s\n", mysql_error(&mysql));
                goto errlbl;
            }
            sqlRow1 = mysql_fetch_row(sqlResult1);
            if (atoi(sqlRow1[0]) == -1 || atoi(sqlRow1[1]) == 0) //用户不在线或未打开群聊窗口
            {
                mysql_real_escape_string(&mysql, messageEsc, message, strnlen(message, 4095));
                sprintf(sqlString, "INSERT INTO GroupMessageTable (GroupId, MessageTime, FromUserName, ToUserName, Message) VALUES ('%s', '%s', '%s', '%s', '%s');", groupIdEsc, messageTime, fromUserNameEsc, toUserNameEsc, messageEsc);
                if (mysql_query(&mysql, sqlString))
                {
                    printf("sql query error: %s\n", mysql_error(&mysql));
                    goto errlbl;
                }
            }
            else
            {
                // #14|2|groupId|FromWhom|Time|Message
                sprintf(messageEsc, "#14|2|%s|%s|%s|%s", groupId, fromUserName, messageTime, message);
                write(atoi(sqlRow1[0]), messageEsc, strnlen(messageEsc, 4095) + 1);
            }
        }
        write(confd, "#14|0", 6); // message stored to db or it has been sent successfully
    }
    mysql_free_result(sqlResult);
    mysql_free_result(sqlResult1);
    return 0;
errlbl:
    write(confd, "#99|1", 6); // sql database error
    mysql_free_result(sqlResult);
    mysql_free_result(sqlResult1);
    return -1;
}

// #15
// request group info
// client->server:#15|0
// (db stores group information into a temp table)
// server->client:#15|0|groupName|nMessages (or #15|1 for invalid connection)
// client->server:#15|1 (for next piece of group info)
// server->client:#15|0|groupName|nMessages (next piece of info)
// client->server:#15|1 (for next piece of group info)
// ............
// server->client:#15|2 (no more info)
int qc_requestGroupInfo(const char* req, int reqLen, int confd)
{
    MYSQL_RES* sqlResult = NULL;
    MYSQL_ROW sqlRow;
    sprintf(sqlString, "SELECT UserName FROM UserInfo WHERE ConnectSocketFD = %d;", confd);
    if (mysql_query(&mysql, sqlString))
    {
        printf("sql query error: %s\n", mysql_error(&mysql));
        goto errlbl;
    }
    sqlResult = mysql_store_result(&mysql);
    if (!sqlResult)
    {
        printf("sql result error: %s\n", mysql_error(&mysql));
        goto errlbl;
    }
    sqlRow = mysql_fetch_row(sqlResult);
    if (!sqlRow)
    {
        write(confd, "#15|1", 6); // invalid connection
    }
    else
    {
        strncpy(toUserName, sqlRow[0], 64);
        mysql_real_escape_string(&mysql, toUserNameEsc, toUserName, strnlen(toUserName, 63));
        if (!(req[4] & 0x0f))
        {
            sprintf(sqlString, "DROP TEMPORARY TABLE IF EXISTS TEMPGroup_%d;", confd);
            if (mysql_query(&mysql, sqlString))
            {
                printf("sql query error: %s\n", mysql_error(&mysql));
                goto errlbl;
            }
            sprintf(sqlString, "CREATE TEMPORARY TABLE TEMPGroup_%d (SELECT GroupId FROM InGroup WHERE UserName = '%s');", confd, toUserNameEsc);
            if (mysql_query(&mysql, sqlString))
            {
                printf("sql query error: %s\n", mysql_error(&mysql));
                goto errlbl;
            }
        }
        sprintf(sqlString, "CREATE TEMPORARY TABLE IF NOT EXISTS TEMPGroup_%d (GroupId VARCHAR(64) NOT NULL);", confd);
        if (mysql_query(&mysql, sqlString))
        {
            printf("sql query error: %s\n", mysql_error(&mysql));
            goto errlbl;
        }
        sprintf(sqlString, "SELECT GroupId FROM TEMPGroup_%d;", confd);
        if (mysql_query(&mysql, sqlString))
        {
            printf("sql query error: %s\n", mysql_error(&mysql));
            goto errlbl;
        }
        mysql_free_result(sqlResult);
        sqlResult = mysql_store_result(&mysql);
        if (!sqlResult)
        {
            printf("sql result error: %s\n", mysql_error(&mysql));
            goto errlbl;
        }
        sqlRow = mysql_fetch_row(sqlResult);
        if (!sqlRow)
        {
            sprintf(sqlString, "DROP TEMPORARY TABLE TEMPGroup_%d;", confd);
            if (mysql_query(&mysql, sqlString))
            {
                printf("sql query error: %s\n", mysql_error(&mysql));
                goto errlbl;
            }
            write(confd, "#15|2", 6); // no more info
        }
        else
        {
            strncpy(groupId, sqlRow[0], 64);
            mysql_real_escape_string(&mysql, groupIdEsc, groupId, strnlen(groupId, 63));
            sprintf(sqlString, "DELETE FROM TEMPGroup_%d WHERE GroupId = '%s';", confd, groupIdEsc);
            if (mysql_query(&mysql, sqlString))
            {
                printf("sql query error: %s\n", mysql_error(&mysql));
                goto errlbl;
            }
            sprintf(sqlString, "SELECT COUNT(*) FROM GroupMessageTable WHERE GroupId = '%s' AND ToUserName = '%s';", groupIdEsc, toUserNameEsc);
            if (mysql_query(&mysql, sqlString))
            {
                printf("sql query error: %s\n", mysql_error(&mysql));
                goto errlbl;
            }
            mysql_free_result(sqlResult);
            sqlResult = mysql_store_result(&mysql);
            if (!sqlResult)
            {
                printf("sql result error: %s\n", mysql_error(&mysql));
                goto errlbl;
            }
            sqlRow = mysql_fetch_row(sqlResult);
            int nMessages = atoi(sqlRow[0]);
            sprintf(message, "#15|0|%s|%d", groupId, nMessages);
            write(confd, message, strnlen(message, 4095) + 1);
        }
    }
    mysql_free_result(sqlResult);
    return 0;
errlbl:
    write(confd, "#99|1", 6); // sql database error
    mysql_free_result(sqlResult);
    return -1;
}

// #16
// open group chat box
// client->server  #16|groupId
// server->client  #16|0|whoSentIt|Time|Message
//                 #16|1 invalid connection
//                 #16|2 end of transmission
// client->server: #16|groupId
// ......and so on
int qc_openGroupChatBox(const char* req, int reqLen, int confd)
{
    MYSQL_RES* sqlResult = NULL;
    MYSQL_ROW sqlRow;
    strncpy(groupId, req + 4, 64);
    sprintf(sqlString, "SELECT UserName FROM UserInfo WHERE ConnectSocketFD = %d;", confd);
    if (mysql_query(&mysql, sqlString))
    {
        printf("sql query error: %s\n", mysql_error(&mysql));
        goto errlbl;
    }
    sqlResult = mysql_store_result(&mysql);
    if (!sqlResult)
    {
        printf("sql result error: %s\n", mysql_error(&mysql));
        goto errlbl;
    }
    sqlRow = mysql_fetch_row(sqlResult);
    if (!sqlRow)
    {
        write(confd, "#16|1", 6); // invalid connection
    }
    else
    {
        mysql_real_escape_string(&mysql, groupIdEsc, groupId, strnlen(groupId, 63));
        mysql_real_escape_string(&mysql, toUserNameEsc, sqlRow[0], strnlen(sqlRow[0], 63));
        sprintf(sqlString, "SELECT FromUserName, MessageTime, Message FROM GroupMessageTable WHERE GroupId = '%s' AND ToUserName = '%s' ORDER BY MessageTime ASC;", groupIdEsc, toUserNameEsc);
        if (mysql_query(&mysql, sqlString))
        {
            printf("sql query error: %s\n", mysql_error(&mysql));
            goto errlbl;
        }
        mysql_free_result(sqlResult);
        sqlResult = mysql_store_result(&mysql);
        if (!sqlResult)
        {
            printf("sql result error: %s\n", mysql_error(&mysql));
            goto errlbl;
        }
        if ((sqlRow = mysql_fetch_row(sqlResult)))
        {
            sprintf(message, "#16|0|%s|%s|%s", sqlRow[0], sqlRow[1], sqlRow[2]); // #16|0|whoSentIt|Time|Message
            write(confd, message, strnlen(message, 4095) + 1);
            mysql_real_escape_string(&mysql, fromUserNameEsc, sqlRow[0], strnlen(sqlRow[0], 63));
            sprintf(sqlString, "DELETE FROM GroupMessageTable WHERE GroupId = '%s' AND MessageTime = '%s' AND FromUserName = '%s' AND ToUserName = '%s';", groupIdEsc, sqlRow[1], fromUserNameEsc, toUserNameEsc);
            if (mysql_query(&mysql, sqlString))
            {
                printf("sql query error: %s\n", mysql_error(&mysql));
                goto errlbl;
            }
        }
        else
        {
            sprintf(sqlString, "UPDATE InGroup SET IsOpen = 1 WHERE GroupId = '%s' AND UserName = '%s';", groupIdEsc, toUserNameEsc);
            if (mysql_query(&mysql, sqlString))
            {
                printf("sql query error: %s\n", mysql_error(&mysql));
                goto errlbl;
            }
            write(confd, "#16|2", 6); // end of transmission
        }
    }
    mysql_free_result(sqlResult);
    return 0;
errlbl:
    write(confd, "#99|1", 6); // sql database error
    mysql_free_result(sqlResult);
    return -1;
}

// #17
// close group chat box
// client->server   #17|groupIdToClose
// server->client   #17|1    invalid connection
//                  #17|0    success
int qc_closeGroupChatBox(const char* req, int reqLen, int confd)
{
    MYSQL_RES* sqlResult = NULL;
    MYSQL_ROW sqlRow;
    strncpy(groupId, req + 4, 64);
    sprintf(sqlString, "SELECT UserName FROM UserInfo WHERE ConnectSocketFD = %d;", confd);
    if (mysql_query(&mysql, sqlString))
    {
        printf("sql query error: %s\n", mysql_error(&mysql));
        goto errlbl;
    }
    sqlResult = mysql_store_result(&mysql);
    if (!sqlResult)
    {
        printf("sql result error: %s\n", mysql_error(&mysql));
        goto errlbl;
    }
    sqlRow = mysql_fetch_row(sqlResult);
    if (!sqlRow)
    {
        write(confd, "#17|1", 6); // invalid connection
    }
    else
    {
        mysql_real_escape_string(&mysql, groupIdEsc, groupId, strnlen(groupId, 63));
        mysql_real_escape_string(&mysql, userNameEsc, sqlRow[0], strnlen(sqlRow[0], 63)); // userName 关闭者
        sprintf(sqlString, "UPDATE InGroup SET IsOpen = 0 WHERE GroupId = '%s' AND UserName = '%s';", groupIdEsc, userNameEsc);
        if (mysql_query(&mysql, sqlString))
        {
            printf("sql query error: %s\n", mysql_error(&mysql));
            goto errlbl;
        }
        write(confd, "#17|0", 6); // group chat box successfully closed
    }
    mysql_free_result(sqlResult);
    return 0;
errlbl:
    write(confd, "#99|1", 6); // sql database error
    mysql_free_result(sqlResult);
    return -1;
}

// #18
// request group member info
// client->server:#18|0|GroupId
// (db stores friend information into a temp table)
// server->client:#18|0|isOnline|userName|imgNo|isGroupOwner (or #18|1 for invalid connection)
// client->server:#18|1|GroupId (for next piece of friend info)
// server->client:#18|0|isOnline|userName|imgNo|isGroupOwner (next piece of info)
// client->server:#18|1|GroupId (for next piece of friend info)
// ............
// server->client:#18|2 (no more info)
int qc_requestGroupMemberInfo(const char* req, int reqLen, int confd)
{
    MYSQL_RES* sqlResult = NULL;
    MYSQL_ROW sqlRow;
    int gidlen = strnlen(strncpy(groupId, req + 6, 64), 63);
    sprintf(sqlString, "SELECT UserName FROM UserInfo WHERE ConnectSocketFD = %d;", confd);
    if (mysql_query(&mysql, sqlString))
    {
        printf("sql query error: %s\n", mysql_error(&mysql));
        goto errlbl;
    }
    sqlResult = mysql_store_result(&mysql);
    if (!sqlResult)
    {
        printf("sql result error: %s\n", mysql_error(&mysql));
        goto errlbl;
    }
    sqlRow = mysql_fetch_row(sqlResult);
    if (!sqlRow)
    {
        write(confd, "#18|1", 6); // invalid connection
    }
    else
    {
        for (int k = 0;k < gidlen;++k)
        {
            fromUserNameEsc[k << 1] = ((groupId[k] >> 4) + 1) | 0x40;
            fromUserNameEsc[(k << 1) + 1] = ((groupId[k] & 0x0f) + 1) | 0x40;
        }
        fromUserNameEsc[gidlen << 1] = 0;
        mysql_real_escape_string(&mysql, groupIdEsc, groupId, strnlen(groupId, 63));
        if (!(req[4] & 0x0f))
        {
            sprintf(sqlString, "DROP TEMPORARY TABLE IF EXISTS TEMPGroup_%d_%s;", confd, fromUserNameEsc);
            if (mysql_query(&mysql, sqlString))
            {
                printf("sql query error: %s\n", mysql_error(&mysql));
                goto errlbl;
            }
            sprintf(sqlString, "CREATE TEMPORARY TABLE TEMPGroup_%d_%s (SELECT UserName, ConnectSocketFD, ImgNo FROM InGroup, UserInfo WHERE InGroup.UserName = UserInfo.UserName AND InGroup.GroupId = '%s');", confd, fromUserNameEsc, groupIdEsc);
            if (mysql_query(&mysql, sqlString))
            {
                printf("sql query error: %s\n", mysql_error(&mysql));
                goto errlbl;
            }
        }
        sprintf(sqlString, "CREATE TEMPORARY TABLE IF NOT EXISTS TEMPGroup_%d_%s (UserName VARCHAR(64) NOT NULL, ConnectSocketFD INT DEFAULT -1, ImgNo INT DEFAULT 0);", confd, fromUserNameEsc);
        if (mysql_query(&mysql, sqlString))
        {
            printf("sql query error: %s\n", mysql_error(&mysql));
            goto errlbl;
        }
        sprintf(sqlString, "SELECT UserName, ConnectSocketFD, ImgNo FROM TEMPGroup_%d_%s;", confd, fromUserNameEsc);
        if (mysql_query(&mysql, sqlString))
        {
            printf("sql query error: %s\n", mysql_error(&mysql));
            goto errlbl;
        }
        mysql_free_result(sqlResult);
        sqlResult = mysql_store_result(&mysql);
        if (!sqlResult)
        {
            printf("sql result error: %s\n", mysql_error(&mysql));
            goto errlbl;
        }
        sqlRow = mysql_fetch_row(sqlResult);
        if (!sqlRow)
        {
            sprintf(sqlString, "DROP TEMPORARY TABLE TEMPGroup_%d_%s;", confd, fromUserNameEsc);
            if (mysql_query(&mysql, sqlString))
            {
                printf("sql query error: %s\n", mysql_error(&mysql));
                goto errlbl;
            }
            write(confd, "#18|2", 6); // no more info
        }
        else
        {
            int userFD = atoi(sqlRow[1]), imgNo = atoi(sqlRow[2]);
            strncpy(userName, sqlRow[0], 64);
            mysql_real_escape_string(&mysql, userNameEsc, userName, strnlen(userName, 63));
            sprintf(sqlString, "DELETE FROM TEMPGroup_%d_%s WHERE UserName = '%s';", confd, fromUserNameEsc, userNameEsc);
            if (mysql_query(&mysql, sqlString))
            {
                printf("sql query error: %s\n", mysql_error(&mysql));
                goto errlbl;
            }
            sprintf(sqlString, "SELECT COUNT(*) FROM GroupInfo WHERE GroupId = '%s' AND GroupOwner = '%s';", groupIdEsc, userNameEsc);
            if (mysql_query(&mysql, sqlString))
            {
                printf("sql query error: %s\n", mysql_error(&mysql));
                goto errlbl;
            }
            mysql_free_result(sqlResult);
            sqlResult = mysql_store_result(&mysql);
            if (!sqlResult)
            {
                printf("sql result error: %s\n", mysql_error(&mysql));
                goto errlbl;
            }
            sqlRow = mysql_fetch_row(sqlResult);
            int isGroupOwner = atoi(sqlRow[0]) ? 1 : 0;
            sprintf(message, "#18|0|%c|%s|%d|%d", userFD != -1 ? '1' : '0', userName, imgNo, isGroupOwner);
            write(confd, message, strnlen(message, 4095) + 1);
        }
    }
    mysql_free_result(sqlResult);
    return 0;
errlbl:
    write(confd, "#99|1", 6); // sql database error
    mysql_free_result(sqlResult);
    return -1;
}

int handleRequest(char* req, int reqLen, int confd)
{
    printf("%s\n", req);
    if (!reqLen)
    {
        handleLogout(confd);
        return -2; // logout
    }
    int reqType;
    sscanf(req, "#%2d|", &reqType);
    switch (reqType)
    {
    case  0:return handleLogout(confd);
    case  1:return qc_signUp(req, reqLen, confd);
    case  2:return qc_login(req, reqLen, confd);
    case  3:return qc_addFriend(req, reqLen, confd);
    case  4:return qc_sendMessage(req, reqLen, confd);
    case  5:return qc_openChatBox(req, reqLen, confd);
    case  6:return qc_requestFriendInfo(req, reqLen, confd);
    case  7:return qc_deleteFriend(req, reqLen, confd);
    case  8:return qc_calcelAccount(confd);
    case  9:return qc_setHeadImage(req, reqLen, confd);
    case 10:return qc_closeChatBox(req, reqLen, confd);
    case 11:return qc_sendFile(req, reqLen, confd);
    case 12:return qc_createGroup(req, reqLen, confd);
    case 13:return qc_applyToGroup(req, reqLen, confd);
    case 14:return qc_sendMessageInGroup(req, reqLen, confd);
    case 15:return qc_requestGroupInfo(req, reqLen, confd);
    case 16:return qc_openGroupChatBox(req, reqLen, confd);
    case 17:return qc_closeGroupChatBox(req, reqLen, confd);
    case 18:return qc_requestGroupMemberInfo(req, reqLen, confd);
    }
    return -1; // invalid request
}

int main()
{
    mysql_init(&mysql);
    if (!mysql_real_connect(&mysql, "localhost", "root", "123456", "qq", 33060, NULL, 0))
    {
        printf("connection error: %s\n", mysql_error(&mysql));
        return 0;
    }

    if (mysql_query(&mysql, "UPDATE UserInfo SET ConnectSocketFD = -1;")
        || mysql_query(&mysql, "UPDATE Friends SET IsOpen = 0;")
        || mysql_query(&mysql, "UPDATE InGroup SET IsOpen = 0;"))
    {
        printf("sql query error (in main): %s\n", mysql_error(&mysql));
        return 0;
    }

    int serverSocketFD = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (serverSocketFD == -1)
    {
        printf("error in socket()\n");
        return 0;
    }

    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY); // inet_addr("127.0.0.1");
    serverAddr.sin_port = htons(1234);

    if (bind(serverSocketFD, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == -1)
    {
        printf("error in bind()\n");
        return 0;
    }

    if (listen(serverSocketFD, SOMAXCONN) == -1)
    {
        printf("error in listen()\n");
        return 0;
    }

    struct pollfd* allFDs = (struct pollfd*)malloc(N_FDS_INIT * sizeof(struct pollfd));
    nfds_t allFDsLen = 1, allFDsCap = N_FDS_INIT, nConnections = 0, i;
    allFDs[0].fd = serverSocketFD;
    allFDs[0].events = POLLIN;

    if (!allFDs)
    {
        printf("error in malloc()\n");
        return 0;
    }

    struct sockaddr_in* connectAddrs = (struct sockaddr_in*)malloc(N_FDS_INIT * sizeof(struct sockaddr_in));
    socklen_t connectAddrLen;
    if (!connectAddrs)
    {
        printf("error in malloc()\n");
        free(allFDs);
        return 0;
    }
    int connectSocketFD;
    static char requestBuffer[SZREQUESTBUFFER];
    while (1)
    {
        int nReady = poll(allFDs, allFDsLen, -1);
        if (nReady == -1)
        {
            printf("error in poll()\n");
            continue;
        }
        if (allFDs[0].revents & POLLIN)
        {
            for (i = 1;allFDs[i].fd != -1 && i < allFDsLen;++i);
            connectAddrLen = sizeof(struct sockaddr_in);
            connectSocketFD = accept(serverSocketFD, (struct sockaddr*)(connectAddrs + i), &connectAddrLen);
            if (connectSocketFD == -1)
            {
                printf("error in accept()\n");
                continue;
            }
            allFDs[i].fd = connectSocketFD;
            allFDs[i].events = POLLIN;
            if (i == allFDsLen)
            {
                ++allFDsLen;
            }
            if (allFDsLen == allFDsCap)
            {
                allFDsCap <<= 1;
                allFDs = (struct pollfd*)realloc(allFDs, allFDsCap * sizeof(struct pollfd));
                connectAddrs = (struct sockaddr_in*)realloc(connectAddrs, allFDsCap * sizeof(struct sockaddr_in));
                if (!(allFDs && connectAddrs))
                {
                    printf("error in realloc()\n");
                    free(allFDs);
                    free(connectAddrs);
                    return 0;
                }
            }
            ++nConnections;
            printf("Connected to client. Client address: %s, port: %d, nConnections: %ld\n", inet_ntoa(connectAddrs[i].sin_addr), ntohs(connectAddrs[i].sin_port), nConnections);
            continue;
        }
        for (i = 1;i < allFDsLen;++i)
        {
            if (allFDs[i].revents & POLLIN)
            {
                connectSocketFD = allFDs[i].fd;
                int nRead = read(connectSocketFD, requestBuffer, SZREQUESTBUFFER);
                if (nRead == -1)
                {
                    printf("error in read()\n");
                    continue;
                }
                requestBuffer[nRead] = 0;
                int rv = handleRequest(requestBuffer, nRead, connectSocketFD);
                if (rv == -2)
                {
                    allFDs[i].fd = -1;
                    while (allFDsLen && allFDs[allFDsLen - 1].fd == -1)
                    {
                        --allFDsLen;
                    }
                    --nConnections;
                    printf("Connection closed. Client address: %s, port: %d, nConnections: %ld\n", inet_ntoa(connectAddrs[i].sin_addr), ntohs(connectAddrs[i].sin_port), nConnections);
                }
                if (rv == -1)
                {
                    printf("An error occurred when handling the request.\n");
                }
            }
        }
    }
    mysql_close(&mysql);
    free(allFDs);
    free(connectAddrs);
    return 0;
}
