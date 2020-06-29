#include "sserver/storage_server_context.h"

StorageServerContext::StorageServerContext():state_(StorageServerContext::state::init),
                                             next_to_transfer_md5_index_(0),
                                             base_path_("./file_storage_dir"){

}
