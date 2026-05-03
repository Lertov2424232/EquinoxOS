import os
import struct
import sys

# Константы EXT2
BLOCK_SIZE = 1024
INODE_SIZE = 128
MAGIC = 0xEF53

def align_to_block(size):
    return (size + BLOCK_SIZE - 1) // BLOCK_SIZE * BLOCK_SIZE

class Ext2Generator:
    def __init__(self, filename, size_mb):
        self.filename = filename
        self.size = size_mb * 1024 * 1024
        self.data = bytearray(self.size)
        
    def write_superblock(self):
        # Суперблок всегда начинается с 1024 байта
        offset = 1024
        s_inodes_count = 1024
        s_blocks_count = self.size // BLOCK_SIZE
        s_free_blocks_count = s_blocks_count - 100 # Примерно
        s_free_inodes_count = s_inodes_count - 11
        
        # Упаковка суперблока (основные поля)
        struct.pack_into("<LLLLLLLLLLHHLLHHHHL", self.data, offset,
            s_inodes_count, s_blocks_count, 0, s_free_blocks_count,
            s_free_inodes_count, 1, 0, 0, 0, 0,
            1, 1, MAGIC, 1, 0, 0, 0, 0, 1024 # 1024 = s_first_ino
        )
        # s_inode_size = 128
        struct.pack_into("<H", self.data, offset + 88, INODE_SIZE)

    def generate(self, source_dir):
        print(f"[EXT2GEN] Creating {self.filename} from {source_dir}...")
        self.write_superblock()
        
        # Группа дескрипторов (начинается со 2-го блока, т.е. 2048)
        # Для простоты: всё в одной группе
        it_offset = 5 * BLOCK_SIZE # Таблица инодов с 5 блока
        self.data_ptr = 20 * BLOCK_SIZE # Данные с 20 блока
        
        # Корневой инод (№2)
        root_inode_offset = it_offset + (1 * INODE_SIZE)
        struct.pack_into("<HHL", self.data, root_inode_offset, 0x41ED, 0, 1024) # Mode: dir, size 1024
        struct.pack_into("<L", self.data, root_inode_offset + 40, 7) # Block[0] = 7 (блок директории)
        
        # Блок корневой директории (Блок 7)
        dir_offset = 7 * BLOCK_SIZE
        # . и ..
        self.add_dir_entry(dir_offset, 0, 2, ".", 1)
        curr_off = self.add_dir_entry(dir_offset, 12, 2, "..", 2)
        
        # Добавляем файлы
        inode_idx = 11 # Пользовательские иноды с 11
        for fname in os.listdir(source_dir):
            fpath = os.path.join(source_dir, fname)
            if os.path.isfile(fpath):
                fsize = os.path.getsize(fpath)
                with open(fpath, "rb") as f:
                    content = f.read()
                
                # Пишем данные
                start_block = self.data_ptr // BLOCK_SIZE
                self.data[self.data_ptr : self.data_ptr + fsize] = content
                self.data_ptr = align_to_block(self.data_ptr + fsize)
                
                # Пишем инод
                ino_off = it_offset + ((inode_idx - 1) * INODE_SIZE)
                struct.pack_into("<HHL", self.data, ino_off, 0x81ED, 0, fsize) # Mode: file
                struct.pack_into("<L", self.data, ino_off + 40, start_block)
                
                # Добавляем в директорию
                curr_off = self.add_dir_entry(dir_offset, curr_off, inode_idx, fname, 1)
                inode_idx += 1
                print(f"  + {fname} ({fsize} bytes) -> Inode {inode_idx-1}")

        with open(self.filename, "wb") as f:
            f.write(self.data)
        print("[EXT2GEN] Done.")

    def add_dir_entry(self, base, offset, ino, name, ftype):
        name_bin = name.encode('ascii')
        name_len = len(name_bin)
        rec_len = (8 + name_len + 3) & ~3
        struct.pack_into("<LHHB", self.data, base + offset, ino, rec_len, name_len, ftype)
        self.data[base + offset + 8 : base + offset + 8 + name_len] = name_bin
        return offset + rec_len

if __name__ == "__main__":
    gen = Ext2Generator("hdd.img", 64)
    gen.generate("iso_root")