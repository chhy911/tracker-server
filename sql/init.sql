-- BitTorrent Tracker Database Initialization

CREATE DATABASE IF NOT EXISTS tracker_db CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;

USE tracker_db;

-- Torrents table
CREATE TABLE IF NOT EXISTS torrents (
    id INT AUTO_INCREMENT PRIMARY KEY,
    info_hash VARCHAR(40) UNIQUE NOT NULL COMMENT 'SHA1 hash of torrent file info',
    name VARCHAR(255) COMMENT 'Torrent name',
    complete INT DEFAULT 0 COMMENT 'Number of peers with complete files',
    incomplete INT DEFAULT 0 COMMENT 'Number of peers with incomplete files',
    downloaded INT DEFAULT 0 COMMENT 'Number of completed downloads',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    INDEX idx_hash (info_hash),
    INDEX idx_created (created_at)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- Peers table
CREATE TABLE IF NOT EXISTS peers (
    id INT AUTO_INCREMENT PRIMARY KEY,
    peer_id VARCHAR(40) NOT NULL COMMENT 'Peer ID',
    torrent_id INT NOT NULL COMMENT 'Reference to torrent',
    info_hash VARCHAR(40) NOT NULL COMMENT 'Torrent info hash',
    ip VARCHAR(15) NOT NULL COMMENT 'Peer IP address',
    port INT NOT NULL COMMENT 'Peer port',
    uploaded BIGINT DEFAULT 0 COMMENT 'Bytes uploaded',
    downloaded BIGINT DEFAULT 0 COMMENT 'Bytes downloaded',
    left_to_download BIGINT DEFAULT 0 COMMENT 'Bytes left to download',
    event VARCHAR(20) COMMENT 'Event: started, stopped, completed',
    last_seen TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    FOREIGN KEY (torrent_id) REFERENCES torrents(id) ON DELETE CASCADE,
    INDEX idx_peer_id (peer_id),
    INDEX idx_hash (info_hash),
    INDEX idx_last_seen (last_seen),
    INDEX idx_torrent (torrent_id),
    UNIQUE KEY unique_peer (torrent_id, peer_id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- Statistics table
CREATE TABLE IF NOT EXISTS statistics (
    id INT AUTO_INCREMENT PRIMARY KEY,
    total_requests BIGINT DEFAULT 0 COMMENT 'Total announce requests',
    total_bytes_uploaded BIGINT DEFAULT 0 COMMENT 'Total bytes uploaded across all peers',
    total_bytes_downloaded BIGINT DEFAULT 0 COMMENT 'Total bytes downloaded across all peers',
    active_peers INT DEFAULT 0 COMMENT 'Current number of active peers',
    total_torrents INT DEFAULT 0 COMMENT 'Total number of torrents',
    timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    INDEX idx_timestamp (timestamp)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

-- Create tracker user
CREATE USER IF NOT EXISTS 'tracker'@'localhost' IDENTIFIED BY 'tracker_password';

-- Grant privileges
GRANT ALL PRIVILEGES ON tracker_db.* TO 'tracker'@'localhost';

-- For remote access (use with caution)
CREATE USER IF NOT EXISTS 'tracker'@'%' IDENTIFIED BY 'tracker_password';
GRANT ALL PRIVILEGES ON tracker_db.* TO 'tracker'@'%';

FLUSH PRIVILEGES;

-- Initial statistics record
INSERT INTO statistics (total_requests, active_peers, total_torrents) 
VALUES (0, 0, 0) ON DUPLICATE KEY UPDATE id=id;