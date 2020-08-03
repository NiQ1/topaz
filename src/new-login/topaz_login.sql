-- phpMyAdmin SQL Dump
-- version 4.3.10
-- http://www.phpmyadmin.net
--
-- Host: localhost
-- Generation Time: Aug 03, 2020 at 09:53 PM
-- Server version: 10.3.12-MariaDB
-- PHP Version: 5.6.6

SET SQL_MODE = "NO_AUTO_VALUE_ON_ZERO";
SET time_zone = "+00:00";


/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;
/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;
/*!40101 SET NAMES utf8 */;

--
-- Database: `topaz_login`
--

-- --------------------------------------------------------

--
-- Table structure for table `accounts`
--

CREATE TABLE IF NOT EXISTS `accounts` (
  `id` int(10) unsigned NOT NULL,
  `username` varchar(128) NOT NULL,
  `password` varchar(128) NOT NULL,
  `salt` varchar(32) NOT NULL,
  `email` varchar(64) DEFAULT NULL,
  `timecreated` datetime NOT NULL DEFAULT current_timestamp(),
  `timemodified` datetime NOT NULL DEFAULT current_timestamp(),
  `expansions` int(11) NOT NULL DEFAULT 4094,
  `features` int(10) unsigned NOT NULL DEFAULT 0,
  `privileges` int(10) unsigned NOT NULL DEFAULT 1
) ENGINE=MyISAM AUTO_INCREMENT=3 DEFAULT CHARSET=utf8;

-- --------------------------------------------------------

--
-- Table structure for table `chars`
--

CREATE TABLE IF NOT EXISTS `chars` (
  `content_id` int(10) unsigned NOT NULL,
  `character_id` int(10) unsigned NOT NULL,
  `name` varchar(16) NOT NULL,
  `world_id` int(10) unsigned NOT NULL,
  `main_job` int(10) unsigned NOT NULL,
  `main_job_lv` int(10) unsigned NOT NULL,
  `zone` int(10) unsigned NOT NULL,
  `race` int(10) unsigned NOT NULL,
  `face` int(10) unsigned NOT NULL,
  `hair` int(11) unsigned NOT NULL,
  `head` int(10) unsigned NOT NULL,
  `body` int(10) unsigned NOT NULL,
  `hands` int(10) unsigned NOT NULL,
  `legs` int(10) unsigned NOT NULL,
  `feet` int(10) unsigned NOT NULL,
  `main` int(10) unsigned NOT NULL,
  `sub` int(10) unsigned NOT NULL,
  `size` int(10) unsigned NOT NULL,
  `nation` int(10) unsigned NOT NULL
) ENGINE=MyISAM DEFAULT CHARSET=utf8;

-- --------------------------------------------------------

--
-- Table structure for table `contents`
--

CREATE TABLE IF NOT EXISTS `contents` (
  `content_id` int(10) unsigned NOT NULL,
  `accout_id` int(10) unsigned NOT NULL,
  `enabled` tinyint(1) NOT NULL DEFAULT 1
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

-- --------------------------------------------------------

--
-- Table structure for table `worlds`
--

CREATE TABLE IF NOT EXISTS `worlds` (
  `id` int(10) unsigned NOT NULL,
  `name` varchar(16) NOT NULL,
  `mq_server_ip` varchar(40) NOT NULL DEFAULT '127.0.0.1',
  `mq_server_port` int(10) unsigned NOT NULL DEFAULT 5672,
  `mq_use_ssl` tinyint(1) NOT NULL DEFAULT 0,
  `mq_ssl_verify_cert` tinyint(1) NOT NULL DEFAULT 0,
  `mq_ssl_ca_cert` mediumblob DEFAULT NULL,
  `mq_ssl_client_cert` mediumblob DEFAULT NULL,
  `mq_ssl_client_key` mediumblob DEFAULT NULL,
  `mq_username` varchar(128) NOT NULL DEFAULT 'guest',
  `mq_password` varchar(128) NOT NULL DEFAULT 'guest',
  `mq_vhost` varchar(128) NOT NULL DEFAULT '/',
  `is_active` tinyint(1) NOT NULL DEFAULT 1,
  `is_test` tinyint(1) NOT NULL DEFAULT 0
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

--
-- Indexes for dumped tables
--

--
-- Indexes for table `accounts`
--
ALTER TABLE `accounts`
  ADD PRIMARY KEY (`id`), ADD UNIQUE KEY `USERNAME` (`username`);

--
-- Indexes for table `chars`
--
ALTER TABLE `chars`
  ADD PRIMARY KEY (`content_id`);

--
-- Indexes for table `contents`
--
ALTER TABLE `contents`
  ADD PRIMARY KEY (`content_id`), ADD UNIQUE KEY `accout_id` (`accout_id`);

--
-- Indexes for table `worlds`
--
ALTER TABLE `worlds`
  ADD PRIMARY KEY (`id`);

--
-- AUTO_INCREMENT for dumped tables
--

--
-- AUTO_INCREMENT for table `accounts`
--
ALTER TABLE `accounts`
  MODIFY `id` int(10) unsigned NOT NULL AUTO_INCREMENT,AUTO_INCREMENT=3;
--
-- AUTO_INCREMENT for table `contents`
--
ALTER TABLE `contents`
  MODIFY `content_id` int(10) unsigned NOT NULL AUTO_INCREMENT;
/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
