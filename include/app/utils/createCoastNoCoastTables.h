#ifndef _APP_UTILS_CREATECOASTNOCOASTTABLES_H_
#define _APP_UTILS_CREATECOASTNOCOASTTABLES_H_

#include <string>

namespace app{
namespace utils{

    /// @brief Fonction pour la création de la table pour le stockage
    /// des parties côtières du landmask
    /// @param areaTableName Nom de la table des unités administratives
    void createCoastTable(std::string const& areaTableName);

    /// @brief Fonction pour la création de la table pour le stockage
    /// des parties non-côtières du landmask
    /// @param areaTableName Nom de la table des unités administratives
    void createNoCoastTable(std::string const& areaTableName);
}
}

#endif