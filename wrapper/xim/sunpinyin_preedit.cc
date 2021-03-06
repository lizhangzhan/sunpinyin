/*
 * Copyright (c) 2010 Mike Qin <mikeandmore@gmail.com>
 *
 * The contents of this file are subject to the terms of either the GNU Lesser
 * General Public License Version 2.1 only ("LGPL") or the Common Development and
 * Distribution License ("CDDL")(collectively, the "License"). You may not use this
 * file except in compliance with the License. You can obtain a copy of the CDDL at
 * http://www.opensource.org/licenses/cddl1.php and a copy of the LGPLv2.1 at
 * http://www.opensource.org/licenses/lgpl-license.php. See the License for the 
 * specific language governing permissions and limitations under the License. When
 * distributing the software, include this License Header Notice in each file and
 * include the full text of the License in the License file as well as the
 * following notice:
 * 
 * NOTICE PURSUANT TO SECTION 9 OF THE COMMON DEVELOPMENT AND DISTRIBUTION LICENSE
 * (CDDL)
 * For Covered Software in this distribution, this License shall be governed by the
 * laws of the State of California (excluding conflict-of-law provisions).
 * Any litigation relating to this License shall be subject to the jurisdiction of
 * the Federal Courts of the Northern District of California and the state courts
 * of the State of California, with venue lying in Santa Clara County, California.
 * 
 * Contributor(s):
 * 
 * If you wish your version of this file to be governed by only the CDDL or only
 * the LGPL Version 2.1, indicate your decision by adding "[Contributor]" elects to
 * include this software in this distribution under the [CDDL or LGPL Version 2.1]
 * license." If you don't indicate a single choice of license, a recipient has the
 * option to distribute your version of this file under either the CDDL or the LGPL
 * Version 2.1, or to extend the choice of license to its licensees as provided
 * above. However, if you add LGPL Version 2.1 code and therefore, elected the LGPL
 * Version 2 license, then the option applies only if the new code is made subject
 * to such option by the copyright holder. 
 */

#include "imi_view.h"
#include "imi_uiobjects.h"
#include "imi_winHandler.h"
#include "imi_options.h"

#include "xim.h"
#include "common.h"

#include <locale.h>

#define BUF_SIZE 4096

template <class UIProvider>
class SSWindowHandler : public CIMIWinHandler, public UIProvider
{
public:
    virtual void updatePreedit(const IPreeditString* ppd);
    virtual void updateCandidates(const ICandidateList* pcl);
    //virtual void updateStatus(int key, int value);
    virtual void commit(const TWCHAR* str);

    void set_xim_handle(XIMHandle* handle) {
        handle_ = handle;
    }

private:
    XIMHandle* handle_;
    
    char buf_[BUF_SIZE];
    TWCHAR front_src[BUF_SIZE];
    TWCHAR end_src[BUF_SIZE];

};

template <class UIProvider> void
SSWindowHandler<UIProvider>::updatePreedit(const IPreeditString* ppd)
{
    TIConvSrcPtr src = (TIConvSrcPtr) (ppd->string());
    memset(front_src, 0, BUF_SIZE * sizeof(TWCHAR));
    memset(end_src, 0, BUF_SIZE * sizeof(TWCHAR));
    
    memcpy(front_src, src, ppd->caret() * sizeof(TWCHAR));
    memcpy(end_src, src + ppd->caret() * sizeof(TWCHAR), 
           (ppd->size() - ppd->caret() + 1) * sizeof(TWCHAR));
    
    memset(buf_, 0, BUF_SIZE);
    
    WCSTOMBS(buf_, front_src, BUF_SIZE - 1);
    buf_[strlen(buf_)] = '|';
    WCSTOMBS(&buf_[strlen(buf_)], end_src, BUF_SIZE - 1);

    // update within the ui provider
    this->update_preedit_ui(ppd, buf_);
}

template <class UIProvider> void
SSWindowHandler<UIProvider>::updateCandidates(const ICandidateList* pcl)
{
    wstring cand_str;
    for (int i = 0, sz = pcl->size(); i < sz; i++) {
        const TWCHAR* pcand = pcl->candiString(i);
        if (pcand == NULL) break;
        cand_str += (i == 9) ? '0' : TWCHAR('1' + i);
        cand_str += TWCHAR('.');
        cand_str += pcand;
        cand_str += TWCHAR(' ');
    }

    TIConvSrcPtr src = (TIConvSrcPtr)(cand_str.c_str());
    WCSTOMBS(buf_, (const TWCHAR*) src, BUF_SIZE - 1);

    // update within the ui provider
    this->update_candidates_ui(pcl, buf_);
}

template <class UIProvider> void
SSWindowHandler<UIProvider>::commit(const TWCHAR* str)
{
    memset(buf_, 0, BUF_SIZE);
    WCSTOMBS(buf_, str, BUF_SIZE - 1);
    if (handle_ != NULL) {
        xim_commit_preedit(handle_, buf_);
    }
}


//TODO: add compiler condition to this
#include "sunpinyin_preedit_gtk.h"
typedef GtkProvider UIProvider;

static SSWindowHandler<UIProvider>* instance = NULL;
static CIMIView* view = NULL;

__EXPORT_API void
preedit_init()
{
    CSunpinyinSessionFactory& fac = CSunpinyinSessionFactory::getFactory();
    view = fac.createSession();

    instance = new SSWindowHandler<UIProvider>();
    view->getIC()->setCharsetLevel(1);// GBK
    view->attachWinHandler(instance);

    view->getHotkeyProfile()->addPageUpKey(CKeyEvent(IM_VK_MINUS));
    view->getHotkeyProfile()->addPageDownKey(CKeyEvent(IM_VK_EQUALS));
}

__EXPORT_API void
preedit_finalize(void)
{
    LOG("preedit_finalizing...");
    CSunpinyinSessionFactory& fac = CSunpinyinSessionFactory::getFactory();    
    fac.destroySession(view);

    if (instance)
        delete instance;
}

__EXPORT_API void
preedit_reload(void)
{
    int ncandi;
    settings_get(CANDIDATES_SIZE, &ncandi);
    view->setCandiWindowSize(ncandi);
    instance->reload_ui();
}

__EXPORT_API void
preedit_set_handle(XIMHandle* handle)
{
    instance->set_xim_handle(handle);
}

__EXPORT_API void
preedit_move(int x, int y)
{
    instance->move(x, y);
}

__EXPORT_API void
preedit_pause(void)
{
    instance->pause();
}

__EXPORT_API void
preedit_go_on(void)
{
    instance->go_on();
}

__EXPORT_API void
preedit_on_key(XIMHandle* handle, unsigned int keycode, unsigned int state)
{
    if (keycode < 0x20 && keycode > 0x7E)
        keycode = 0;
    view->onKeyEvent(CKeyEvent(keycode, keycode, state));
}

__EXPORT_API bool
preedit_status(void)
{
    return instance->status();
}

