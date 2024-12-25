-- service mysql start
-- mysql -u root -p
-- source server.sql

-- service mysql stop

DROP DATABASE IF EXISTS qq;
CREATE DATABASE qq;

USE qq;

CREATE TABLE UserInfo(
    UserName VARCHAR(64) NOT NULL PRIMARY KEY,
    UserPassword VARCHAR(64) NOT NULL,
    ConnectSocketFD INT DEFAULT -1,  -- -1 means not online
    ImgNo INT DEFAULT 0
);

CREATE TABLE Friends(
    UserA VARCHAR(64) NOT NULL,
    UserB VARCHAR(64) NOT NULL,
    IsOpen INT DEFAULT 0
);

CREATE TABLE MessageTable(
    MessageTime DATETIME NOT NULL,
    FromUserName VARCHAR(64) NOT NULL,
    ToUserName VARCHAR(64) NOT NULL,
    Message VARCHAR(4096)
);

CREATE TABLE GroupInfo(
    GroupId VARCHAR(64) NOT NULL,
    GroupOwner VARCHAR(64) NOT NULL
);

CREATE TABLE GroupMessageTable(
    GroupId VARCHAR(64) NOT NULL,
    MessageTime DATETIME NOT NULL,
    FromUserName VARCHAR(64) NOT NULL,
    ToUserName VARCHAR(64) NOT NULL,
    Message VARCHAR(4096)
);

CREATE TABLE InGroup(
    GroupId VARCHAR(64) NOT NULL,
    UserName VARCHAR(64) NOT NULL,
    IsOpen INT DEFAULT 0
);
