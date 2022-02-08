#ifndef BUNDLE_VIEW_V6_H
#define BUNDLE_VIEW_V6_H 1

#include "codec/bpv6.h"
#include <cstdint>
#include <vector>
#include <list>
#include <map>
#include <boost/asio/buffer.hpp>

/*

Each bundle shall be a concatenated sequence of at least two block
structures.  The first block in the sequence must be a primary bundle
block, and no bundle may have more than one primary bundle block.
Additional bundle protocol blocks of other types may follow the
primary block to support extensions to the bundle protocol, such as
the Bundle Security Protocol [BSP].  At most one of the blocks in the
sequence may be a payload block.  The last block in the sequence must
have the "last block" flag (in its block processing control flags)
set to 1; for every other block in the bundle after the primary
block, this flag must be set to zero.

To keep from possibly invalidating bundle security, the sequencing
of the blocks in a forwarded bundle must not be changed as it
transits a node; received blocks must be transmitted in the same
relative order as that in which they were received.  While blocks
may be added to bundles as they transit intermediate nodes,
removal of blocks that do not have their 'Discard block if it
can't be processed' flag in the block processing control flags set
to 1 may cause security to fail.

Bundle security must not be invalidated by forwarding nodes even
though they themselves might not use the Bundle Security Protocol.
In particular, the sequencing of the blocks in a forwarded bundle
must not be changed as it transits a node; received blocks must be
transmitted in the same relative order as that in which they were
received.  While blocks may be added to bundles as they transit
intermediate nodes, removal of blocks that do not have their 'Discard
block if it can't be processed' flag in the block processing control
flags set to 1 may cause security to fail.

A bundle MAY have multiple security blocks.
*/

class BundleViewV6 {

public:
    struct Bpv6PrimaryBlockView {
        Bpv6CbhePrimaryBlock header;
        boost::asio::const_buffer actualSerializedPrimaryBlockPtr;
        bool dirty;

        void SetManuallyModified();
    };
    struct Bpv6CanonicalBlockView {
        bpv6_canonical_block header;
        boost::asio::const_buffer actualSerializedHeaderAndBodyPtr;
        boost::asio::const_buffer actualSerializedBodyPtr;
        std::vector<uint8_t> replacementBlockBodyData;
        bool dirty;
        bool markedForDeletion;

        void SetManuallyModified();
        void SetBlockProcessingControlFlagAndDirtyIfNecessary(const uint64_t flag);
        void ClearBlockProcessingControlFlagAndDirtyIfNecessary(const uint64_t flag);
        bool HasBlockProcessingControlFlagSet(const uint64_t flag) const;
        
    };
    //typedef std::multimap<uint8_t, Bpv6CanonicalBlockView>::iterator canonical_block_view_iterator_t;
    //typedef std::pair<canonical_block_view_iterator_t, canonical_block_view_iterator_t> canonical_block_view_range_t;

    BundleViewV6();
    ~BundleViewV6();

    void AppendCanonicalBlock(const bpv6_canonical_block & header, std::vector<uint8_t> & blockBody);
    //canonical_block_view_range_t GetCanonicalBlockRangeByType(const uint8_t canonicalBlockTypeCode);
    std::size_t GetCanonicalBlockCountByType(const uint8_t canonicalBlockTypeCode) const;
    std::size_t GetNumCanonicalBlocks() const;
    void GetCanonicalBlocksByType(const uint8_t canonicalBlockTypeCode, std::vector<Bpv6CanonicalBlockView*> & blocks);
    std::size_t DeleteAllCanonicalBlocksByType(const uint8_t canonicalBlockTypeCode);
    bool LoadBundle(uint8_t * bundleData, const std::size_t size, const bool loadPrimaryBlockOnly = false);
    bool SwapInAndLoadBundle(std::vector<uint8_t> & bundleData, const bool loadPrimaryBlockOnly = false);
    bool CopyAndLoadBundle(const uint8_t * bundleData, const std::size_t size, const bool loadPrimaryBlockOnly = false);
    bool IsValid() const;
    bool Render(const std::size_t maxBundleSizeBytes);
private:
    bool Load(const bool loadPrimaryBlockOnly);
    void Reset();
    
public:
    Bpv6PrimaryBlockView m_primaryBlockView;
    const uint8_t * m_applicationDataUnitStartPtr;
    std::list<Bpv6CanonicalBlockView> m_listCanonicalBlockView; //list will maintain block relative order


    boost::asio::const_buffer m_renderedBundle;
    std::vector<uint8_t> m_frontBuffer;
    std::vector<uint8_t> m_backBuffer;
};

#endif // BUNDLE_VIEW_V6_H

