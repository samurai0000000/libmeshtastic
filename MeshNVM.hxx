/*
 * MeshNVM.hxx
 *
 * Copyright (C) 2025, Charles Chiou
 */

#ifndef MESHNVM_HXX
#define MESHNVM_HXX

#include <BaseNVM.hxx>

using namespace std;

class MeshNVM : public BaseNVM {

public:

    MeshNVM();
    ~MeshNVM();

    bool setupFor(uint32_t node_num);

    virtual bool loadNvm(void);
    virtual bool saveNvm(void);

    bool hasNvmChanged(void) const {
        return _changed;
    }

    inline void clearNvmChanged(void) {
        _changed = false;
    }

protected:

    uint32_t _node_num;
    string _path;
    bool _changed;

};

#endif

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
