#ifndef _APP_UTILS_CREATECOASTNOCOASTTABLES_H_
#define _APP_UTILS_CREATECOASTNOCOASTTABLES_H_

#include <string>

namespace app{
namespace utils{

    /// @brief Fonction pour la création de la table pour le stockage
    /// des parties côtières du landmask
    void createCoastTable();

    /// @brief Fonction pour la création de la table pour le stockage
    /// des parties non-côtières du landmask
    void createNoCoastTable();
}
}

#endif