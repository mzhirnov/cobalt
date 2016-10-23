#pragma once

// Classes in this file:
//	asset
//	asset_manager

#include "cool/common.hpp"
#include "cool/background_worker.hpp"

class asset_manager;

// asset
class asset : public ref_counter<asset> {
public:
	asset(asset_manager* manager) : _manager(manager) {}
	virtual ~asset() {}

	virtual void load() = 0;
	virtual void unload() = 0;
	
	virtual bool is_loading() const = 0;
	virtual bool is_loaded() const = 0;

	virtual size_t calculate_memory_in_use() const = 0;

protected:
	asset_manager* _manager;

private:
	DISALLOW_COPY_AND_ASSIGN(asset);
};

typedef boost::intrusive_ptr<asset> asset_ptr;

// asset manager
class asset_manager {
public:
	asset_manager();

private:

private:
	DISALLOW_COPY_AND_ASSIGN(asset_manager);
};
