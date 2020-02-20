#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <getopt.h>

#define MBR_SIZE 512
#define START_OFFSET 0x1BE
#define PARTITION_ENTRY_SIZE 16
#define PARTITION_ENTRIES 4
#define PARTITION_ENTRIES_SIZE (PARTITION_ENTRY_SIZE * PARTITION_ENTRIES)
#define SIGNATURE_SIZE 2
#define SIGNATURE 0x55AAu
#define BOOTABLE_MARKER 0x80u
#define NON_BOOTABLE_MARKER 0x00u

int main(int argc, char *argv[])
{
	int fd = -1;
	unsigned char buf[MBR_SIZE] = { 0 };
	int bytes_transferred = 0;
	int set_partition = -1;
	int index;

	while ((index = getopt (argc, argv, "s:")) != -1)
	{
		switch (index)
		{
			case 's':
				set_partition = atoi(optarg);
				break;
			default:
				abort();
		}
	}

	if ((argc - optind) != 1 || (set_partition != -1 && (set_partition < 1 || set_partition > 4)))
	{
		fprintf(stderr, "Usage: %s [-s partition_to_set] block_device\n", argv[0]);
		exit(1);
	}

	if (set_partition != -1)
	{
		--set_partition;
	}

	if ((fd = open(argv[optind], set_partition == -1 ? O_RDONLY: O_RDWR)) < 0) {
		fprintf(stderr, "Error opening block device: %s, %s\n", argv[optind], strerror(errno));
		exit(1);
	}

	while (bytes_transferred < sizeof(buf))
	{
		int ret = read(fd, &buf[bytes_transferred], sizeof(buf) - bytes_transferred);
		if (ret < 0)
		{
			fprintf(stderr, "Error reading block device: %s\n", strerror(errno));
			exit(1);
		}

		bytes_transferred += ret;
	}

	if (((buf[sizeof(buf) - 2] << 8) | (buf[sizeof(buf) - 1])) != SIGNATURE)
	{
		fprintf(stderr, "Invalid MBR record\n");
		exit(1);
	}

	if (set_partition == -1)
	{
		int bootable_partition = -1;
		for (int i = 0; i < PARTITION_ENTRIES; ++i)
		{
			if (buf[START_OFFSET + (i * PARTITION_ENTRY_SIZE)] == BOOTABLE_MARKER)
			{
				if (bootable_partition != -1)
				{
					fprintf(stderr, "Error multiple boot partitions found\n");
					exit(1);
				}

				bootable_partition = i;
			}
		}

		if (bootable_partition != -1)
		{
			printf("%d\n", bootable_partition + 1);
		}
	}
	else
	{
		for (int i = 0; i < PARTITION_ENTRIES; ++i)
		{
			buf[START_OFFSET + (i * PARTITION_ENTRY_SIZE)] = (i == set_partition) ? BOOTABLE_MARKER : NON_BOOTABLE_MARKER;
		}

		bytes_transferred = 0;
		while (bytes_transferred < sizeof(buf))
		{
			int ret = pwrite(fd, buf, sizeof(buf) - bytes_transferred, 0 + bytes_transferred);
			if (ret < 0)
			{
				fprintf(stderr, "Error writing block device: %s\n", strerror(errno));
				exit(1);
			}

			bytes_transferred += ret;
		}

		printf("Updated bootable partition\n");
	}
}
