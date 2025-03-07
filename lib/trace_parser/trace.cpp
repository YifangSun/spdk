/*-
 *   BSD LICENSE
 *
 *   Copyright (c) Intel Corporation.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "spdk/stdinc.h"
#include "spdk/log.h"
#include "spdk/trace_parser.h"
#include "spdk/util.h"

#include <exception>
#include <map>
#include <new>

struct entry_key {
	entry_key(uint16_t _lcore, uint64_t _tsc) : lcore(_lcore), tsc(_tsc) {}
	uint16_t lcore;
	uint64_t tsc;
};

class compare_entry_key
{
public:
	bool operator()(const entry_key &first, const entry_key &second) const
	{
		if (first.tsc == second.tsc) {
			return first.lcore < second.lcore;
		} else {
			return first.tsc < second.tsc;
		}
	}
};

typedef std::map<entry_key, spdk_trace_entry *, compare_entry_key> entry_map;

struct spdk_trace_parser {
	spdk_trace_parser(const spdk_trace_parser_opts *opts);
	~spdk_trace_parser();
	spdk_trace_parser(const spdk_trace_parser &) = delete;
	spdk_trace_parser &operator=(const spdk_trace_parser &) = delete;
	const spdk_trace_flags *flags() const { return &_histories->flags; }
	uint64_t tsc_offset() const { return _tsc_offset; }
	bool next_entry(spdk_trace_parser_entry *entry);
private:
	void populate_events(spdk_trace_history *history, int num_entries);
	bool init(const spdk_trace_parser_opts *opts);
	void cleanup();

	spdk_trace_histories	*_histories;
	size_t			_map_size;
	int			_fd;
	uint64_t		_tsc_offset;
	entry_map		_entries;
	entry_map::iterator	_iter;
};

bool
spdk_trace_parser::next_entry(spdk_trace_parser_entry *entry)
{
	if (_iter == _entries.end()) {
		return false;
	}

	entry->entry = _iter->second;
	entry->lcore = _iter->first.lcore;

	_iter++;
	return true;
}

void
spdk_trace_parser::populate_events(spdk_trace_history *history, int num_entries)
{
	int i, num_entries_filled;
	spdk_trace_entry *e;
	int first, last, lcore;

	lcore = history->lcore;
	e = history->entries;

	num_entries_filled = num_entries;
	while (e[num_entries_filled - 1].tsc == 0) {
		num_entries_filled--;
	}

	if (num_entries == num_entries_filled) {
		first = last = 0;
		for (i = 1; i < num_entries; i++) {
			if (e[i].tsc < e[first].tsc) {
				first = i;
			}
			if (e[i].tsc > e[last].tsc) {
				last = i;
			}
		}
	} else {
		first = 0;
		last = num_entries_filled - 1;
	}

	/*
	 * We keep track of the highest first TSC out of all reactors.
	 *  We will ignore any events that occured before this TSC on any
	 *  other reactors.  This will ensure we only print data for the
	 *  subset of time where we have data across all reactors.
	 */
	if (e[first].tsc > _tsc_offset) {
		_tsc_offset = e[first].tsc;
	}

	i = first;
	while (1) {
		if (e[i].tpoint_id != SPDK_TRACE_MAX_TPOINT_ID) {
			_entries[entry_key(lcore, e[i].tsc)] = &e[i];
		}
		if (i == last) {
			break;
		}
		i++;
		if (i == num_entries_filled) {
			i = 0;
		}
	}
}

bool
spdk_trace_parser::init(const spdk_trace_parser_opts *opts)
{
	spdk_trace_history *history;
	struct stat st;
	int rc, i;

	switch (opts->mode) {
	case SPDK_TRACE_PARSER_MODE_FILE:
		_fd = open(opts->filename, O_RDONLY);
		break;
	case SPDK_TRACE_PARSER_MODE_SHM:
		_fd = shm_open(opts->filename, O_RDONLY, 0600);
		break;
	default:
		SPDK_ERRLOG("Invalid mode: %d\n", opts->mode);
		return false;
	}

	if (_fd < 0) {
		SPDK_ERRLOG("Could not open trace file: %s (%d)\n", opts->filename, errno);
		return false;
	}

	rc = fstat(_fd, &st);
	if (rc < 0) {
		SPDK_ERRLOG("Could not get size of trace file: %s\n", opts->filename);
		return false;
	}

	if ((size_t)st.st_size < sizeof(*_histories)) {
		SPDK_ERRLOG("Invalid trace file: %s\n", opts->filename);
		return false;
	}

	/* Map the header of trace file */
	_map_size = sizeof(*_histories);
	_histories = static_cast<spdk_trace_histories *>(mmap(NULL, _map_size, PROT_READ,
			MAP_SHARED, _fd, 0));
	if (_histories == MAP_FAILED) {
		SPDK_ERRLOG("Could not mmap trace file: %s\n", opts->filename);
		_histories = NULL;
		return false;
	}

	/* Remap the entire trace file */
	_map_size = spdk_get_trace_histories_size(_histories);
	munmap(_histories, sizeof(*_histories));
	if ((size_t)st.st_size < _map_size) {
		SPDK_ERRLOG("Trace file %s is not valid\n", opts->filename);
		_histories = NULL;
		return false;
	}
	_histories = static_cast<spdk_trace_histories *>(mmap(NULL, _map_size, PROT_READ,
			MAP_SHARED, _fd, 0));
	if (_histories == MAP_FAILED) {
		SPDK_ERRLOG("Could not mmap trace file: %s\n", opts->filename);
		_histories = NULL;
		return false;
	}

	if (opts->lcore == SPDK_TRACE_MAX_LCORE) {
		for (i = 0; i < SPDK_TRACE_MAX_LCORE; i++) {
			history = spdk_get_per_lcore_history(_histories, i);
			if (history->num_entries == 0 || history->entries[0].tsc == 0) {
				continue;
			}

			populate_events(history, history->num_entries);
		}
	} else {
		history = spdk_get_per_lcore_history(_histories, opts->lcore);
		if (history->num_entries > 0 && history->entries[0].tsc != 0) {
			populate_events(history, history->num_entries);
		}
	}

	_iter = _entries.begin();
	return true;
}

void
spdk_trace_parser::cleanup()
{
	if (_histories != NULL) {
		munmap(_histories, _map_size);
	}

	if (_fd > 0) {
		close(_fd);
	}
}

spdk_trace_parser::spdk_trace_parser(const spdk_trace_parser_opts *opts) :
	_histories(NULL),
	_map_size(0),
	_fd(-1),
	_tsc_offset(0)
{
	if (!init(opts)) {
		cleanup();
		throw std::exception();
	}
}

spdk_trace_parser::~spdk_trace_parser()
{
	cleanup();
}

struct spdk_trace_parser *
spdk_trace_parser_init(const struct spdk_trace_parser_opts *opts)
{
	try {
		return new spdk_trace_parser(opts);
	} catch (...) {
		return NULL;
	}
}

void
spdk_trace_parser_cleanup(struct spdk_trace_parser *parser)
{
	delete parser;
}

const struct spdk_trace_flags *
spdk_trace_parser_get_flags(const struct spdk_trace_parser *parser)
{
	return parser->flags();
}

uint64_t
spdk_trace_parser_get_tsc_offset(const struct spdk_trace_parser *parser)
{
	return parser->tsc_offset();
}

bool
spdk_trace_parser_next_entry(struct spdk_trace_parser *parser,
			     struct spdk_trace_parser_entry *entry)
{
	return parser->next_entry(entry);
}
